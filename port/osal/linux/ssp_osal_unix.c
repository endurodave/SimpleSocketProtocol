// Implements the SSP OSAL (operation system abstraction layer) interface within ssp_osal.h 
// for the Windows operating system

#include "ssp_opt.h"


#if (SSP_OSAL == SSP_OSAL_UNIX)
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

#include <time.h>
//#include <timeop.h>
#include <sys/time.h>

#include "ssp_osal.h"
#include "ssp_fault.h"

static pthread_mutex_t g_mcritical = PTHREAD_MUTEX_INITIALIZER;


void SSPOSAL_Init(void)
{
    if(pthread_mutex_init(&g_mcritical,NULL) < 0)
        ASSERT_TRUE(0);
}

void SSPOSAL_Term(void)
{
    pthread_mutex_destroy(&g_mcritical);
}

void SSPOSAL_EnterCritical(void)
{
    if(pthread_mutex_lock(&g_mcritical)<0)
    	ASSERT_TRUE(0);
}

void SSPOSAL_ExitCritical(void)
{
    if(pthread_mutex_unlock(&g_mcritical)<0)
    	ASSERT_TRUE(0);
}

SSP_OSAL_HANDLE SSPOSAL_LockCreate(void)
{
	pthread_mutex_t *my_mutex;
	my_mutex = malloc(sizeof(pthread_mutex_t));
    if(pthread_mutex_init(my_mutex,NULL) < 0)
        ASSERT_TRUE(0);
    return (void*)my_mutex;
}

void SSPOSAL_LockDestroy(SSP_OSAL_HANDLE handle)
{
	pthread_mutex_destroy((pthread_mutex_t*)handle);
}

BOOL SSPOSAL_LockGet(SSP_OSAL_HANDLE handle, UINT32 timeout)
{

	if (timeout != SSP_OSAL_WAIT_INFINITE)
	{
		// Absolute deadline: now + timeout ms, with nanoseconds normalized
		struct timespec when;
		clock_gettime(CLOCK_REALTIME, &when);
		when.tv_sec += timeout / 1000;
		when.tv_nsec += (long)(timeout % 1000) * 1000000L;
		if (when.tv_nsec >= 1000000000L)
		{
			when.tv_sec += 1;
			when.tv_nsec -= 1000000000L;
		}
		if(pthread_mutex_timedlock((pthread_mutex_t*)handle, &when)!=0)
			return FALSE;
	}
	else
	{
		if(pthread_mutex_lock((pthread_mutex_t*)handle) != 0)
			return FALSE;
	}

    return TRUE;
}

BOOL SSPOSAL_LockPut(SSP_OSAL_HANDLE handle)
{
    if(pthread_mutex_unlock((pthread_mutex_t*)handle)<0)
    	return FALSE;
    return TRUE;
}

static UINT32 get_milliseconds() 
{
    struct timeval tv;
    
    // Get the current time
    if (gettimeofday(&tv, NULL) == -1) {
        perror("gettimeofday");
        return 0; // Return 0 if there was an error
    }

    // Convert seconds to milliseconds and add microseconds (microseconds / 1000)
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

UINT32 SSPOSAL_GetTickCount(void)
{
	//return (UINT32)millis(0);
	
	return get_milliseconds();
}

#endif  //#if (SSP_OSAL == SSP_OSAL_UNIX)
