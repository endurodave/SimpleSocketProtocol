// SSP OSAL (operating system abstration layer) interface. Implement each function
// based on target system. 

#ifndef SSP_OSAL_H
#define SSP_OSAL_H

#include "ssp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* SSP_OSAL_HANDLE;

#define SSP_OSAL_WAIT_INFINITE      (0xffffffffUL)
#define SSP_OSAL_WAIT_DEFAULT       (5000)

#define SSP_OSAL_INVALID_HANDLE_VALUE    (SSP_OSAL_HANDLE)-1

void SSPOSAL_Init(void);
void SSPOSAL_Term(void);

void SSPOSAL_EnterCritical(void);
void SSPOSAL_ExitCritical(void);

SSP_OSAL_HANDLE SSPOSAL_LockCreate(void);
void SSPOSAL_LockDestroy(SSP_OSAL_HANDLE handle);
BOOL SSPOSAL_LockGet(SSP_OSAL_HANDLE handle, UINT32 timeout);
BOOL SSPOSAL_LockPut(SSP_OSAL_HANDLE handle);

UINT32 SSPOSAL_GetTickCount(void);

#ifdef __cplusplus
}
#endif

#endif  // SSP_OSAL_H

