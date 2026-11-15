// Implements the SSP HAL (hardware abstraction layer) interface within ssp_hal.h
// for an Arduino serial port.
//
// Each SSP port uses a Stream. At run time (any IDE), call
// SSP_ArduinoSetPort() from ssp_arduino.h before SSP_Init(). Otherwise the
// defaults are used, overridable with build flags (e.g. in platformio.ini):
//   SSP_ARDUINO_PORT1     serial object for SSP_PORT1 (default Serial1 where it exists)
//   SSP_ARDUINO_PORT2     serial object for SSP_PORT2 (default Serial2 where it exists)
//   SSP_ARDUINO_NO_PORT2  no default for port 2, e.g. a board with one UART
//   SSP_ARDUINO_BAUD      baud rate for the defaults (default 115200)
// For example -DSSP_ARDUINO_PORT1=Serial -DSSP_ARDUINO_NO_PORT2 -DSSP_NO_TRACE
// runs SSP over the USB serial port to a PC (trace off: it would share the port).
// A port with no stream fails to open (SSP_Init returns SSP_PORT_OPEN_FAILED).

#include "ssp_opt.h"

#if (SSP_HAL == SSP_HAL_ARDUINO)

#include "ssp_hal.h"
#include "ssp_osal.h"
#include "ssp_fault.h"
#include "ssp_arduino.h"
#include <Arduino.h>

// Default serial objects: Serial1/Serial2 on cores known to have them
#if defined(ARDUINO_ARCH_SAM) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_RP2040)
#define SSP_HAS_SERIAL1
#define SSP_HAS_SERIAL2
#else
#if defined(HAVE_HWSERIAL1)     // AVR and STM32 cores define these
#define SSP_HAS_SERIAL1
#endif
#if defined(HAVE_HWSERIAL2)
#define SSP_HAS_SERIAL2
#endif
#endif

#if !defined(SSP_ARDUINO_PORT1) && defined(SSP_HAS_SERIAL1)
#define SSP_ARDUINO_PORT1   Serial1
#endif
#if !defined(SSP_ARDUINO_PORT2) && defined(SSP_HAS_SERIAL2) && !defined(SSP_ARDUINO_NO_PORT2)
#define SSP_ARDUINO_PORT2   Serial2
#endif
#ifdef SSP_ARDUINO_NO_PORT2
#undef SSP_ARDUINO_PORT2
#endif
#ifndef SSP_ARDUINO_BAUD
#define SSP_ARDUINO_BAUD    115200
#endif

#define SSP_LOCK_WAIT_DEFAULT   5000

static BOOL powerSave = TRUE;
static BOOL initDone = FALSE;
static Stream* streams[SSP_MAX_PORTS];          // set by the sketch or at open
static BOOL ownsStream[SSP_MAX_PORTS];          // opened (begin) here: end it on close
static BOOL portOpen[SSP_MAX_PORTS];
static SSP_OSAL_HANDLE hSerialLock[SSP_MAX_PORTS];

void SSP_ArduinoSetPort(SspPortId portId, Stream& stream)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);
    streams[portId] = &stream;
    ownsStream[portId] = FALSE;
}

// Begin the default serial object for a port; NULL if the port has none
static Stream* OpenDefault(SspPortId portId)
{
    switch (portId)
    {
#ifdef SSP_ARDUINO_PORT1
    case SSP_PORT1:
        SSP_ARDUINO_PORT1.begin(SSP_ARDUINO_BAUD);
        return &SSP_ARDUINO_PORT1;
#endif
#ifdef SSP_ARDUINO_PORT2
    case SSP_PORT2:
        SSP_ARDUINO_PORT2.begin(SSP_ARDUINO_BAUD);
        return &SSP_ARDUINO_PORT2;
#endif
    default:
        return NULL;
    }
}

static void CloseDefault(SspPortId portId)
{
    switch (portId)
    {
#ifdef SSP_ARDUINO_PORT1
    case SSP_PORT1:
        SSP_ARDUINO_PORT1.end();
        break;
#endif
#ifdef SSP_ARDUINO_PORT2
    case SSP_PORT2:
        SSP_ARDUINO_PORT2.end();
        break;
#endif
    default:
        break;
    }
}

void SSPHAL_Init(SspPortId portId)
{
    (void)portId;
    // SSP calls this once per port: create the locks on the first call only
    if (initDone)
        return;
    initDone = TRUE;

    for (INT port = 0; port < SSP_MAX_PORTS; port++)
    {
        hSerialLock[port] = SSPOSAL_LockCreate();
        ASSERT_TRUE(hSerialLock[port] != SSP_OSAL_INVALID_HANDLE_VALUE);
    }
}

void SSPHAL_Term(void)
{
    if (!initDone)
        return;

    for (INT port = SSP_PORT1; port < SSP_MAX_PORTS; port++)
    {
        if (portOpen[port])
            SSPHAL_PortClose((SspPortId)port);
    }
    for (INT port = 0; port < SSP_MAX_PORTS; port++)
    {
        SSPOSAL_LockDestroy(hSerialLock[port]);
        hSerialLock[port] = SSP_OSAL_INVALID_HANDLE_VALUE;
    }
    initDone = FALSE;
}

BOOL SSPHAL_PortOpen(SspPortId portId)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);
    if (streams[portId] == NULL)
    {
        streams[portId] = OpenDefault(portId);
        ownsStream[portId] = streams[portId] != NULL ? TRUE : FALSE;
    }
    portOpen[portId] = streams[portId] != NULL ? TRUE : FALSE;
    SSPOSAL_LockPut(hSerialLock[portId]);
    return portOpen[portId];
}

void SSPHAL_PortClose(SspPortId portId)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);
    if (ownsStream[portId])
    {
        CloseDefault(portId);
        streams[portId] = NULL;
        ownsStream[portId] = FALSE;
    }
    portOpen[portId] = FALSE;
    SSPOSAL_LockPut(hSerialLock[portId]);
}

BOOL SSPHAL_PortIsOpen(SspPortId portId)
{
    // Tracked here: SSP_Process() asks about every port, including ones that
    // were never opened (or don't exist on this board)
    if (portId < SSP_PORT1 || portId >= SSP_MAX_PORTS)
        return FALSE;
    return portOpen[portId];
}

BOOL SSPHAL_PortSend(SspPortId portId, const char* buf, UINT16 bytesToSend)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);

    if (NULL == buf || 0 == bytesToSend || !portOpen[portId])
        return FALSE;

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);
    size_t bytesSent = streams[portId]->write((const uint8_t*)buf, bytesToSend);
    SSPOSAL_LockPut(hSerialLock[portId]);

    return bytesSent == bytesToSend ? TRUE : FALSE;
}

BOOL SSPHAL_PortRecv(SspPortId portId, char* buf, UINT16* bytesRead, UINT16 maxLen, UINT16 timeout)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);
    (void)timeout;

    if (NULL == buf || NULL == bytesRead)
        return FALSE;

    *bytesRead = 0;
    if (maxLen == 0 || !portOpen[portId])
        return FALSE;

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);
    *bytesRead = (UINT16)streams[portId]->readBytes(buf, maxLen);
    SSPOSAL_LockPut(hSerialLock[portId]);
    return TRUE;
}

BOOL SSPHAL_IsRecvQueueEmpty(SspPortId portId)
{
    if (portId < SSP_PORT1 || portId >= SSP_MAX_PORTS || !portOpen[portId])
        return TRUE;

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);
    int available = streams[portId]->available();
    SSPOSAL_LockPut(hSerialLock[portId]);

    // If receive queue is empty
    return available <= 0 ? TRUE : FALSE;
}

void SSPHAL_PortFlush(SspPortId portId)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);

    if (!portOpen[portId])
        return;

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);
    streams[portId]->flush();
    SSPOSAL_LockPut(hSerialLock[portId]);
}

void SSPHAL_PowerSave(BOOL enable)
{
    powerSave = enable;

    if (FALSE == powerSave)
    {
        /// @TODO: Do something when power savings disabled
    }
}

BOOL SSPHAL_IsPowerSave(void)
{
    return powerSave;
}

#endif
