// Implements the SSP HAL (hardware abstraction layer) interface within ssp_hal.h
// for an Arduino serial port.
//
// Build options (e.g. build_flags in platformio.ini):
//   SSP_ARDUINO_PORT1     serial object for SSP_PORT1 (default Serial1)
//   SSP_ARDUINO_PORT2     serial object for SSP_PORT2 (default Serial2)
//   SSP_ARDUINO_NO_PORT2  define for a single port, e.g. a board with one UART
//   SSP_ARDUINO_BAUD      baud rate (default 115200)
// For example -DSSP_ARDUINO_PORT1=Serial -DSSP_ARDUINO_NO_PORT2 -DSSP_NO_TRACE
// runs SSP over the USB serial port to a PC (trace off: it would share the port).

#include "ssp_opt.h"

#if (SSP_HAL == SSP_HAL_ARDUINO)

#include "ssp_hal.h"
#include "ssp_osal.h"
#include "ssp_fault.h"
#include <Arduino.h>

#ifndef SSP_ARDUINO_PORT1
#define SSP_ARDUINO_PORT1   Serial1
#endif
#ifndef SSP_ARDUINO_PORT2
#define SSP_ARDUINO_PORT2   Serial2
#endif
#ifndef SSP_ARDUINO_BAUD
#define SSP_ARDUINO_BAUD    115200
#endif

#define SSP_LOCK_WAIT_DEFAULT   5000

// Run 'statement' with 'port' as the serial object for portId
#ifdef SSP_ARDUINO_NO_PORT2
#define WITH_PORT(portId, port, statement) \
    switch (portId) { \
    case SSP_PORT1: { auto& port = SSP_ARDUINO_PORT1; statement; break; } \
    default: ASSERT(); }
#else
#define WITH_PORT(portId, port, statement) \
    switch (portId) { \
    case SSP_PORT1: { auto& port = SSP_ARDUINO_PORT1; statement; break; } \
    case SSP_PORT2: { auto& port = SSP_ARDUINO_PORT2; statement; break; } \
    default: ASSERT(); }
#endif

static BOOL powerSave = TRUE;
static BOOL initDone = FALSE;
static BOOL portOpen[SSP_MAX_PORTS];
static SSP_OSAL_HANDLE hSerialLock[SSP_MAX_PORTS];

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
    WITH_PORT(portId, port, port.begin(SSP_ARDUINO_BAUD))
    portOpen[portId] = TRUE;
    SSPOSAL_LockPut(hSerialLock[portId]);
    return TRUE;
}

void SSPHAL_PortClose(SspPortId portId)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);
    WITH_PORT(portId, port, port.end())
    portOpen[portId] = FALSE;
    SSPOSAL_LockPut(hSerialLock[portId]);
}

BOOL SSPHAL_PortIsOpen(SspPortId portId)
{
    // Tracked here: SSP_Process() asks about every port, including ones that
    // were never opened (or, with SSP_ARDUINO_NO_PORT2, don't exist)
    if (portId < SSP_PORT1 || portId >= SSP_MAX_PORTS)
        return FALSE;
    return portOpen[portId];
}

BOOL SSPHAL_PortSend(SspPortId portId, const char* buf, UINT16 bytesToSend)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);

    if (NULL == buf || 0 == bytesToSend)
        return FALSE;

    size_t bytesSent = 0;
    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);
    WITH_PORT(portId, port, bytesSent = port.write((const uint8_t*)buf, bytesToSend))
    SSPOSAL_LockPut(hSerialLock[portId]);

    return bytesSent == bytesToSend ? TRUE : FALSE;
}

BOOL SSPHAL_PortRecv(SspPortId portId, char* buf, UINT16* bytesRead, UINT16 maxLen, UINT16 timeout)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);

    if (NULL == buf || NULL == bytesRead)
        return FALSE;

    *bytesRead = 0;
    if (maxLen == 0)
        return FALSE;

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);
    WITH_PORT(portId, port, *bytesRead = (UINT16)port.readBytes(buf, maxLen))
    SSPOSAL_LockPut(hSerialLock[portId]);
    return TRUE;
}

BOOL SSPHAL_IsRecvQueueEmpty(SspPortId portId)
{
    int available = 0;
    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);
    WITH_PORT(portId, port, available = port.available())
    SSPOSAL_LockPut(hSerialLock[portId]);

    // If receive queue is empty
    return available == 0 ? TRUE : FALSE;
}

void SSPHAL_PortFlush(SspPortId portId)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);
    WITH_PORT(portId, port, port.flush())
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
