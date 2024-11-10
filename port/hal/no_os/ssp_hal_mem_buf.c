// Implements the SSP HAL (hardware abstraction layer) interface within ssp_hal.h 
// using memory buffers to simulate communication between sockets for testing SSP

#include "ssp_opt.h"
#include <stdio.h>
#include <stdlib.h>

#if (SSP_HAL == SSP_HAL_MEM_BUF)

#include "ssp_hal.h"
#include "ssp_osal.h"

#define SEND_RETRY_MAX      2
#define SEND_RETRY_DELAY    5   // in mS

#define RECV_BUF_SIZE    1024 

// Define to periodically corrupt data for testing
//#define CORRUPT_DATA_TEST

static UINT8 recvDataBuf[SSP_MAX_PORTS][RECV_BUF_SIZE];
static INT recvDataHead[SSP_MAX_PORTS];
static INT recvDataTail[SSP_MAX_PORTS];

static BOOL powerSave = TRUE;

void SSPHAL_Init(SspPortId portId)
{
}

void SSPHAL_Term(void)
{
}

BOOL SSPHAL_PortOpen(SspPortId portId)
{
    return TRUE;
}

void SSPHAL_PortClose(SspPortId portId)
{
}

BOOL SSPHAL_PortIsOpen(SspPortId portId)
{
    return TRUE;
}

BOOL SSPHAL_PortSend(SspPortId portId, const char* buf, UINT16 bytesToSend)
{
    if (NULL == buf || 0 == bytesToSend)
        return FALSE;

    BOOL success = TRUE; 

    SSPOSAL_EnterCritical();

#ifdef CORRUPT_DATA_TEST
    static INT corruptCnt = 0;
    INT byteToCorrupt = 0;
    INT bitToCorrupt = 0;
    BOOL corruptData = FALSE;
    if (corruptCnt++ % 5 == 0)
    {
        corruptData = TRUE;
        byteToCorrupt = rand() % bytesToSend;
        bitToCorrupt = rand() % 8;
        printf("### Corrupt data sent. Port: %d Byte: %d Bit: %d ###\n", portId, byteToCorrupt, bitToCorrupt);
    }
#endif

    // Loopback port 1 to port 2
    if (portId == SSP_PORT1)
        portId = SSP_PORT2;
    else
        portId = SSP_PORT1;

    for (int b = 0; b < bytesToSend; b++)
    {
        // Buffer full?
        if (recvDataHead[portId] - recvDataTail[portId] == -1 ||
            (recvDataHead[portId] == RECV_BUF_SIZE - 1 && recvDataTail[portId] == 0))
        {
            success = FALSE;
            break;
        }

        // Copy data to recv memory buffer
        recvDataBuf[portId][recvDataHead[portId]] = buf[b];

#ifdef CORRUPT_DATA_TEST
        if (corruptData && b == byteToCorrupt)
        {
            // Flip random bit
            BYTE b = 0x1 << bitToCorrupt;
            if (b & recvDataBuf[portId][recvDataHead[portId]])
                recvDataBuf[portId][recvDataHead[portId]] &= ~b;
            else
                recvDataBuf[portId][recvDataHead[portId]] |= b;
        }
#endif

        // Wrap buffer index if end reached
        if (++recvDataHead[portId] >= RECV_BUF_SIZE)
            recvDataHead[portId] = 0;
    }

    SSPOSAL_ExitCritical();

    return success;
}

BOOL SSPHAL_PortRecv(SspPortId portId, char* buf, UINT16* bytesRead, UINT16 maxLen, UINT16 timeout)
{
    if (NULL == buf || NULL == bytesRead)
        return FALSE;

    *bytesRead = 0;

    SSPOSAL_EnterCritical();

    for (int b = 0; b < maxLen; b++)
    {
        // Buffer empty?
        if (recvDataHead[portId] == recvDataTail[portId])
            break;

        // Copy byte into caller's buffer
        buf[b] = recvDataBuf[portId][recvDataTail[portId]];

        // Increment caller's bytes read
        (*bytesRead)++;

        // Wrap buffer index if end reached
        if (++recvDataTail[portId] >= RECV_BUF_SIZE)
            recvDataTail[portId] = 0;
    }

    SSPOSAL_ExitCritical();

    return TRUE;
}

BOOL SSPHAL_IsRecvQueueEmpty(SspPortId portId)
{
    if (recvDataHead[portId] == recvDataTail[portId])
        return TRUE;
    else
        return FALSE;
}

void SSPHAL_PortFlush(SspPortId portId)
{
    recvDataHead[portId] = recvDataTail[portId];
}

void SSPHAL_PowerSave(BOOL enable)
{
    powerSave = enable;

    if (FALSE == powerSave)
    {
        /// @TODO: Do something when power savings disabled as necessary
    }
}

BOOL SSPHAL_IsPowerSave(void)
{
    return powerSave;
}

#endif

