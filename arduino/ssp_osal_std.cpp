// Implements the SSP OSAL (operation system abstraction layer) interface within ssp_osal.h 
// using the C++ standard library

#include "ssp_opt.h"

#if (SSP_OSAL == SSP_OSAL_STD)

#include "ssp_osal.h"
#include "ssp_fault.h"
#include <mutex>
#include <chrono>

static std::mutex _mutex;

void SSPOSAL_Init(void)
{
}

void SSPOSAL_Term(void)
{
}

void SSPOSAL_EnterCritical(void)
{
    _mutex.lock();
}

void SSPOSAL_ExitCritical(void)
{
    _mutex.unlock();
}

SSP_OSAL_HANDLE SSPOSAL_LockCreate(void)
{
    std::timed_mutex* m = new std::timed_mutex();
    ASSERT_TRUE(m != NULL);

    SSP_OSAL_HANDLE hMutex = (SSP_OSAL_HANDLE)m;
    return hMutex;
}

void SSPOSAL_LockDestroy(SSP_OSAL_HANDLE handle)
{
    ASSERT_TRUE(handle != NULL);
    std::timed_mutex* m = (std::timed_mutex*)handle;
    delete m;
}

BOOL SSPOSAL_LockGet(SSP_OSAL_HANDLE handle, UINT32 timeout)
{
    ASSERT_TRUE(handle != NULL);
    std::timed_mutex* m = (std::timed_mutex*)handle;

    auto now = std::chrono::steady_clock::now();
    if (m->try_lock_until(now + std::chrono::milliseconds(timeout)))
        return TRUE;
    else
        return FALSE;
}

BOOL SSPOSAL_LockPut(SSP_OSAL_HANDLE handle)
{
    ASSERT_TRUE(handle != NULL);
    std::timed_mutex* m = (std::timed_mutex*)handle;
    m->unlock();
    return TRUE;
}

UINT32 SSPOSAL_GetTickCount(void)
{
    using namespace std::chrono;
    auto mslonglong = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    UINT32 mslong = (UINT32)mslonglong;
    return mslong;
}

#endif


