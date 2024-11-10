// Implements the SSP OSAL (operation system abstraction layer) interface within ssp_osal.h 
// for the Windows operating system

#include "ssp_opt.h"

#if (SSP_OSAL == SSP_OSAL_WIN)

#include "ssp_osal.h"
#include "ssp_fault.h"

static CRITICAL_SECTION hLock;

void SSPOSAL_Init(void)
{
    BOOL lockSuccess = InitializeCriticalSectionAndSpinCount(&hLock, 0x00000400);
    ASSERT_TRUE(lockSuccess != 0);
}

void SSPOSAL_Term(void)
{
    DeleteCriticalSection(&hLock);
}

void SSPOSAL_EnterCritical(void)
{
    EnterCriticalSection(&hLock);
}

void SSPOSAL_ExitCritical(void)
{
    LeaveCriticalSection(&hLock);
}

SSP_OSAL_HANDLE SSPOSAL_LockCreate(void)
{
    HANDLE hMutex = CreateMutex(NULL, FALSE, NULL);
    ASSERT_TRUE(hMutex != INVALID_HANDLE_VALUE);
    return hMutex;
}

void SSPOSAL_LockDestroy(SSP_OSAL_HANDLE handle)
{
    BOOL success = CloseHandle(handle);
    ASSERT_TRUE(success);
}

BOOL SSPOSAL_LockGet(SSP_OSAL_HANDLE handle, UINT32 timeout)
{
    DWORD dwTimeout = timeout;
    if (timeout == SSP_OSAL_WAIT_INFINITE)
        dwTimeout = INFINITE;

    HANDLE hMutex = (HANDLE)handle;
    DWORD e = WaitForSingleObject(hMutex, dwTimeout);
    if (e != WAIT_OBJECT_0)
        return FALSE;
    return TRUE;
}

BOOL SSPOSAL_LockPut(SSP_OSAL_HANDLE handle)
{
    HANDLE hMutex = (HANDLE)handle;
    BOOL success = ReleaseMutex(hMutex);
    if (success == FALSE)
        return FALSE;
    return TRUE;
}

UINT32 SSPOSAL_GetTickCount(void)
{
    return GetTickCount();
}

#endif

