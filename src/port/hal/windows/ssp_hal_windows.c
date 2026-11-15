// Implements the SSP HAL (hardware abstraction layer) interface within ssp_hal.h
// for a Windows COM port. Select the COM port with SSPHAL_SetPortName()
// (ssp_hal_serial.h) before SSP_Init(); defaults are COM1 and COM2.

#include "ssp_opt.h"

#if (SSP_HAL == SSP_HAL_WIN)

#include "ssp_hal.h"
#include "ssp_hal_serial.h"
#include "ssp_osal.h"
#include "ssp_fault.h"
#include <stdio.h>
#include <string.h>

#define SSP_LOCK_WAIT_DEFAULT   5000
#define PORT_NAME_MAX           32

static BOOL powerSave = TRUE;
static BOOL initDone = FALSE;
static HANDLE m_hCommFile[SSP_MAX_PORTS];
static SSP_OSAL_HANDLE hSerialLock[SSP_MAX_PORTS];
static DWORD m_baudRate = CBR_115200;
static char m_portName[SSP_MAX_PORTS][PORT_NAME_MAX] = { "", "COM1", "COM2" };

void SSPHAL_SetPortName(SspPortId portId, const char* name)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS && name != NULL);
    strncpy(m_portName[portId], name, PORT_NAME_MAX - 1);
    m_portName[portId][PORT_NAME_MAX - 1] = 0;
}

void SSPHAL_SetBaudRate(UINT32 baudRate)
{
    m_baudRate = baudRate;
}

//----------------------------------------------------------------------------
// ConfigureComPort
//----------------------------------------------------------------------------
static BOOL ConfigureComPort(SspPortId portId)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);

    DCB dcb;
    FillMemory(&dcb, sizeof(dcb), 0);
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(m_hCommFile[portId], &dcb))
        return FALSE;

    dcb.BaudRate = m_baudRate;
    dcb.fBinary = TRUE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fAbortOnError = FALSE;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.ByteSize = 8;
    if (!SetCommState(m_hCommFile[portId], &dcb))
        return FALSE;

    COMMTIMEOUTS commtimeouts;
    if (GetCommTimeouts(m_hCommFile[portId], &commtimeouts) == 0)
        return FALSE;

    commtimeouts.ReadIntervalTimeout = 0;
    commtimeouts.ReadTotalTimeoutMultiplier = 0;
    commtimeouts.ReadTotalTimeoutConstant = 500;
    commtimeouts.WriteTotalTimeoutMultiplier = 5;
    commtimeouts.WriteTotalTimeoutConstant = 500;

    if (SetCommTimeouts(m_hCommFile[portId], &commtimeouts) == 0)
        return FALSE;

    return TRUE;
}

void SSPHAL_Init(SspPortId portId)
{
    (void)portId;
    // SSP calls this once per port: set up all ports on the first call only,
    // so initializing port 2 doesn't drop port 1's open handle
    if (initDone)
        return;
    initDone = TRUE;

    for (INT port = 0; port < SSP_MAX_PORTS; port++)
    {
        m_hCommFile[port] = INVALID_HANDLE_VALUE;
        hSerialLock[port] = SSPOSAL_LockCreate();
        ASSERT_TRUE(hSerialLock[port] != SSP_OSAL_INVALID_HANDLE_VALUE);
    }
}

void SSPHAL_Term(void)
{
    if (!initDone)
        return;

    for (INT port = SSP_PORT1; port < SSP_MAX_PORTS; port++)
        SSPHAL_PortClose((SspPortId)port);
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

    if (SSPHAL_PortIsOpen(portId) == TRUE)
        return TRUE;

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    // The \\.\ prefix is required for COM10 and above
    char path[PORT_NAME_MAX + 8];
    snprintf(path, sizeof(path), "\\\\.\\%s", m_portName[portId]);
    m_hCommFile[portId] = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);

    BOOL success = FALSE;
    if (m_hCommFile[portId] != INVALID_HANDLE_VALUE)
        success = ConfigureComPort(portId);

    SSPOSAL_LockPut(hSerialLock[portId]);

    // A missing or busy port is reported to the caller (SSP_PORT_OPEN_FAILED)
    if (success == FALSE)
        SSPHAL_PortClose(portId);

    return success;
}

void SSPHAL_PortClose(SspPortId portId)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    if (m_hCommFile[portId] != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hCommFile[portId]);
        m_hCommFile[portId] = INVALID_HANDLE_VALUE;
    }

    SSPOSAL_LockPut(hSerialLock[portId]);
}

BOOL SSPHAL_PortIsOpen(SspPortId portId)
{
    BOOL isOpen;
    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    if (m_hCommFile[portId] == INVALID_HANDLE_VALUE)
        isOpen = FALSE;
    else
        isOpen = TRUE;

    SSPOSAL_LockPut(hSerialLock[portId]);
    return isOpen;
}

BOOL SSPHAL_PortSend(SspPortId portId, const char* buf, UINT16 bytesToSend)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);

    if (NULL == buf || 0 == bytesToSend)
        return FALSE;

    BOOL success = FALSE;
    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    if (m_hCommFile[portId] != INVALID_HANDLE_VALUE)
    {
        DWORD bytesSent = 0;
        success = WriteFile(m_hCommFile[portId], buf, bytesToSend, &bytesSent, NULL);
        if (bytesSent != bytesToSend)
            success = FALSE;
    }

    SSPOSAL_LockPut(hSerialLock[portId]);
    return success ? TRUE : FALSE;
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

    BOOL success = FALSE;
    COMMTIMEOUTS commtimeouts;
    if (m_hCommFile[portId] != INVALID_HANDLE_VALUE &&
        GetCommTimeouts(m_hCommFile[portId], &commtimeouts) != 0)
    {
        commtimeouts.ReadTotalTimeoutConstant = timeout;
        if (SetCommTimeouts(m_hCommFile[portId], &commtimeouts) != 0)
        {
            DWORD read = 0;
            success = ReadFile(m_hCommFile[portId], buf, maxLen, &read, NULL);
            *bytesRead = (UINT16)read;
        }
    }

    SSPOSAL_LockPut(hSerialLock[portId]);
    return success ? TRUE : FALSE;
}

BOOL SSPHAL_IsRecvQueueEmpty(SspPortId portId)
{
    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    // Get the COM port errors and stats
    DWORD errors;
    COMSTAT comStat;
    BOOL success = ClearCommError(m_hCommFile[portId], &errors, &comStat);

    SSPOSAL_LockPut(hSerialLock[portId]);

    // Port closed or failed, or receive queue empty
    if (!success || comStat.cbInQue == 0)
        return TRUE;
    else
        return FALSE;
}

void SSPHAL_PortFlush(SspPortId portId)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    PurgeComm(m_hCommFile[portId], PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);

    // Clear the port of COM errors
    DWORD errors;
    COMSTAT comStat;
    ClearCommError(m_hCommFile[portId], &errors, &comStat);

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
