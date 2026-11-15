// SSP HAL (hardware abstration layer) interface. Implement each function
// based on target system. 

#ifndef SSP_HAL_H
#define SSP_HAL_H

#include "ssp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

void SSPHAL_Init(SspPortId portId);
void SSPHAL_Term(void);

BOOL SSPHAL_PortOpen(SspPortId portId);
void SSPHAL_PortClose(SspPortId portId);
BOOL SSPHAL_PortIsOpen(SspPortId portId);

BOOL SSPHAL_PortSend(SspPortId portId, const char* buf, UINT16 bytesToSend);
BOOL SSPHAL_PortRecv(SspPortId portId, char* buf, UINT16* bytesRead, UINT16 maxLen, UINT16 timeout);

void SSPHAL_PortFlush(SspPortId portId);
BOOL SSPHAL_IsRecvQueueEmpty(SspPortId portId);

void SSPHAL_PowerSave(BOOL enable);
BOOL SSPHAL_IsPowerSave(void);

#ifdef __cplusplus
}
#endif

#endif // SSP_HAL_H
