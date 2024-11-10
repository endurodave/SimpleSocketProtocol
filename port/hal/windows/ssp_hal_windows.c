// Implements the SSP HAL (hardware abstraction layer) interface within ssp_hal.h 
// for a Windows COM port

#include "ssp_opt.h"

#if (SSP_HAL == SSP_HAL_WIN)

#include "ssp_hal.h"
#include "ssp_osal.h"
#include "ssp_fault.h"

#define SSP_LOCK_WAIT_DEFAULT   5000

static BOOL powerSave = TRUE;
static HANDLE m_hCommFile[SSP_MAX_PORTS];
static SSP_OSAL_HANDLE hSerialLock[SSP_MAX_PORTS];
static DWORD m_baudRate = CBR_115200;

//----------------------------------------------------------------------------
// SetBaudRate
//----------------------------------------------------------------------------
static BOOL SetBaudRate(SspPortId portId, DWORD baudRate)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId <= SSP_MAX_PORTS);
    m_baudRate = baudRate;

    DCB dcb;
    FillMemory(&dcb, sizeof(dcb), 0);
    if (!GetCommState(m_hCommFile[portId], &dcb))
        return FALSE;

    dcb.BaudRate = m_baudRate;
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
    {
        DWORD e = GetLastError();
        return FALSE;
    }

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
    for (INT port = 0; port < SSP_MAX_PORTS; port++)
    {
        m_hCommFile[port] = INVALID_HANDLE_VALUE;
        hSerialLock[port] = SSPOSAL_LockCreate();
        ASSERT_TRUE(hSerialLock[port] != SSP_OSAL_INVALID_HANDLE_VALUE);
    }
}

void SSPHAL_Term(void)
{
    for (INT port = 0; port < SSP_MAX_PORTS; port++)
    {
        SSPHAL_PortClose(port);
        SSPOSAL_LockDestroy(hSerialLock[port]);
        hSerialLock[port] = SSP_OSAL_INVALID_HANDLE_VALUE;
    }
}

BOOL SSPHAL_PortOpen(SspPortId portId)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId <= SSP_MAX_PORTS);

    if (SSPHAL_PortIsOpen(portId) == TRUE)
        return TRUE;

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    switch (portId)
    {
    case SSP_PORT1:
        m_hCommFile[portId] = CreateFile(L"COM1:", GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
        break;
    case SSP_PORT2:
        m_hCommFile[portId] = CreateFile(L"COM2:", GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
        break;
    default:
        ASSERT();
    }

    ASSERT_TRUE(m_hCommFile[portId] != INVALID_HANDLE_VALUE);

    BOOL success = SetBaudRate(portId, m_baudRate);

    SSPOSAL_LockPut(hSerialLock[portId]);

    if (success == FALSE)
        SSPHAL_PortClose(portId);

    return success;
}

void SSPHAL_PortClose(SspPortId portId)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId <= SSP_MAX_PORTS);

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
    ASSERT_TRUE(portId >= SSP_PORT1 && portId <= SSP_MAX_PORTS);

    if (NULL == buf || 0 == bytesToSend)
        return FALSE;

    BOOL success = FALSE;
    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    if (m_hCommFile[portId] != INVALID_HANDLE_VALUE)
    {
        DWORD bytesSent;
        success = WriteFile(m_hCommFile[portId], buf, bytesToSend, &bytesSent, NULL);
        ASSERT_TRUE(bytesToSend == bytesSent);
    }

    SSPOSAL_LockPut(hSerialLock[portId]);
    return success ? TRUE : FALSE;
}

BOOL SSPHAL_PortRecv(SspPortId portId, char* buf, UINT16* bytesRead, UINT16 maxLen, UINT16 timeout)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId <= SSP_MAX_PORTS);

    if (NULL == buf || NULL == bytesRead)
        return FALSE;

    if (maxLen <= 0)
        return FALSE;

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    BOOL success = FALSE;
    *bytesRead = 0;

    COMMTIMEOUTS commtimeouts;
    if (GetCommTimeouts(m_hCommFile[portId], &commtimeouts) == 0)
        goto error;
    commtimeouts.ReadTotalTimeoutConstant = timeout;
    if (SetCommTimeouts(m_hCommFile[portId], &commtimeouts) == 0)
        goto error;

    // Read data from serial port
    success = ReadFile(m_hCommFile[portId], buf, maxLen, (DWORD*)bytesRead, NULL);

    SSPOSAL_LockPut(hSerialLock[portId]);
    return success ? TRUE : FALSE;

error:
    SSPOSAL_LockPut(hSerialLock[portId]);
    return FALSE;
}

BOOL SSPHAL_IsRecvQueueEmpty(SspPortId portId)
{
    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    // Get the COM port errors and stats
    DWORD errors;
    COMSTAT comStat;
    BOOL success = ClearCommError(m_hCommFile[portId], &errors, &comStat);
    ASSERT_TRUE(success == TRUE);

    SSPOSAL_LockPut(hSerialLock[portId]);

    // If receive queue is empty
    if (comStat.cbInQue == 0)
        return TRUE;
    else
        return FALSE;
}

void SSPHAL_PortFlush(SspPortId portId)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId <= SSP_MAX_PORTS);

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    PurgeComm(m_hCommFile[portId], PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);

    // Clear the port of COM errors 
    DWORD errors;
    COMSTAT comStat;
    BOOL success = ClearCommError(m_hCommFile[portId], &errors, &comStat);
    ASSERT_TRUE(success == TRUE);

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

