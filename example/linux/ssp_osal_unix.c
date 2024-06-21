// Implements the SSP OSAL (operation system abstraction layer) interface within ssp_osal.h 
// for the Windows operating system

#include "ssp_opt.h"


#if (SSP_OSAL == SSP_OSAL_UNIX)
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

#include <time.h>
#include <timeop.h>

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
		struct timespec now, when;
		timespec_get(&now, TIME_UTC);

		UINT32 when_tv_sec,when_tv_nsec;

		when_tv_sec = timeout/1000;				//Calc secs
		when_tv_nsec = (timeout*1000)%1000; 	//Calc nanos

		when.tv_sec = now.tv_sec + when_tv_sec;
		when.tv_nsec = now.tv_nsec + when_tv_sec;
		if(pthread_mutex_timedlock((pthread_mutex_t*)handle, &when)!=0)
			return FALSE;
	}
	else
	{
		if(pthread_mutex_lock(&g_mcritical) < 0)
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

UINT32 SSPOSAL_GetTickCount(void)
{
	return (UINT32)millis(0);
}

#endif  //#if (SSP_OSAL == SSP_OSAL_UNIX)
