// Implements the SSP HAL (hardware abstraction layer) interface within ssp_hal.h 
// for an Arduino serial port

#include "ssp_opt.h"

#if (SSP_HAL == SSP_HAL_ARDUINO)

#include "ssp_hal.h"
#include "ssp_osal.h"
#include "ssp_fault.h"
#include <Arduino.h>

#define SSP_LOCK_WAIT_DEFAULT   5000

static BOOL powerSave = TRUE;
static SSP_OSAL_HANDLE hSerialLock[SSP_MAX_PORTS];

void SSPHAL_Init(SspPortId portId)
{
    for (INT port = 0; port < SSP_MAX_PORTS; port++)
    {
        hSerialLock[port] = SSPOSAL_LockCreate();
        ASSERT_TRUE(hSerialLock[port] != SSP_OSAL_INVALID_HANDLE_VALUE);
    }
}

void SSPHAL_Term(void)
{
    for (INT port = 0; port < SSP_MAX_PORTS; port++)
    {
        SSPHAL_PortClose((SspPortId)port);
        SSPOSAL_LockDestroy(hSerialLock[port]);
        hSerialLock[port] = SSP_OSAL_INVALID_HANDLE_VALUE;
    }
}

BOOL SSPHAL_PortOpen(SspPortId portId)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId <= SSP_MAX_PORTS);

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    switch (portId)
    {
    case SSP_PORT1:
        Serial1.begin(115200);
        break;
    case SSP_PORT2:
        Serial2.begin(115200);
        break;
    default:
        ASSERT();
    }

    SSPOSAL_LockPut(hSerialLock[portId]);
    return TRUE;
}

void SSPHAL_PortClose(SspPortId portId)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId <= SSP_MAX_PORTS);

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    switch (portId)
    {
    case SSP_PORT1:
        Serial1.end();
        break;
    case SSP_PORT2:
        Serial2.end();
        break;
    default:
        ASSERT();
    }

    SSPOSAL_LockPut(hSerialLock[portId]);
}

BOOL SSPHAL_PortIsOpen(SspPortId portId)
{
    BOOL isOpen;
    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    switch (portId)
    {
    case SSP_PORT1:
        isOpen = Serial1;
        break;
    case SSP_PORT2:
        isOpen = Serial2;
        break;
    default:
        ASSERT();
    }

    SSPOSAL_LockPut(hSerialLock[portId]);
    return isOpen;
}

BOOL SSPHAL_PortSend(SspPortId portId, const char* buf, UINT16 bytesToSend)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId <= SSP_MAX_PORTS);

    if (NULL == buf || 0 == bytesToSend)
        return FALSE;

    int bytesSent = 0;
    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    switch (portId)
    {
    case SSP_PORT1:
        bytesSent = Serial1.write(buf, bytesToSend);
        break;
    case SSP_PORT2:
        bytesSent = Serial2.write(buf, bytesToSend);
        break;
    default:
        ASSERT();
    }

    SSPOSAL_LockPut(hSerialLock[portId]);

    if (bytesSent == bytesToSend)
        return TRUE;
    else
        return FALSE; 
}

BOOL SSPHAL_PortRecv(SspPortId portId, char* buf, UINT16* bytesRead, UINT16 maxLen, UINT16 timeout)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId <= SSP_MAX_PORTS);

    if (NULL == buf || NULL == bytesRead)
        return FALSE;

    if (maxLen <= 0)
        return FALSE;

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    switch (portId)
    {
    case SSP_PORT1:
        *bytesRead = Serial1.readBytes(buf, maxLen);
        break;
    case SSP_PORT2:
        *bytesRead = Serial2.readBytes(buf, maxLen);
        break;
    default:
        ASSERT();
    }

    SSPOSAL_LockPut(hSerialLock[portId]);
    return TRUE;

error:
    SSPOSAL_LockPut(hSerialLock[portId]);
    return FALSE;
}

BOOL SSPHAL_IsRecvQueueEmpty(SspPortId portId)
{
    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    int available = 0;
    switch (portId)
    {
    case SSP_PORT1:
        available = Serial1.available();
        break;
    case SSP_PORT2:
        available = Serial2.available();
        break;
    default:
        ASSERT();
    }

    SSPOSAL_LockPut(hSerialLock[portId]);

    // If receive queue is empty
    if (available == 0)
        return TRUE;
    else
        return FALSE;
}

void SSPHAL_PortFlush(SspPortId portId)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId <= SSP_MAX_PORTS);

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    switch (portId)
    {
    case SSP_PORT1:
        Serial1.flush();
        break;
    case SSP_PORT2:
        Serial2.flush();
        break;
    default:
        ASSERT();
    }

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

