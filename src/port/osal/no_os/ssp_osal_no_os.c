// Implements the SSP OSAL (operation system abstraction layer) interface within ssp_osal.h 
// for systems without an operating system

#include "ssp_opt.h"
#include <time.h>

#if (SSP_OSAL == SSP_OSAL_NO_OS)

#include "ssp_osal.h"

#ifdef ARDUINO
// Arduino's millis() (declared in the C++ Arduino.h). Without this declaration
// C assumes it returns int, which is 16 bits on AVR: the tick count then reads
// the wrong half of the 32-bit value and ACK timeouts never expire.
unsigned long millis(void);
#endif

void SSPOSAL_Init(void)
{
}

void SSPOSAL_Term(void)
{
}

void SSPOSAL_EnterCritical(void)
{
}

void SSPOSAL_ExitCritical(void)
{
}

SSP_OSAL_HANDLE SSPOSAL_LockCreate(void)
{
    SSP_OSAL_HANDLE hSema = NULL;
    return hSema;
}

void SSPOSAL_LockDestroy(SSP_OSAL_HANDLE handle)
{
    (void)handle;
}

BOOL SSPOSAL_LockGet(SSP_OSAL_HANDLE handle, UINT32 timeout)
{
    (void)handle;
    (void)timeout;
    return TRUE;
}

BOOL SSPOSAL_LockPut(SSP_OSAL_HANDLE handle)
{
    (void)handle;
    return TRUE;
}

UINT32 SSPOSAL_GetTickCount(void)
{
    UINT32 count;

    /// @TODO: Implement a time-based tick counter for no operating system
#ifdef ARDUINO
    count = millis();
#else
    clock_t c = clock() / (CLOCKS_PER_SEC / 1000);
    count = (UINT32)c;
#endif
    return count;
}

#endif


