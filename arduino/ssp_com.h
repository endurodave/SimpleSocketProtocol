// SSP module to send messages and parse received messages. 

#ifndef SSP_COM_H
#define SSP_COM_H

#include "ssp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SspData SspData;

// Called one time per port to initialize 
SspErr SSPCOM_Init(SspPortId portId);

// Called one time at application end
void SSPCOM_Term(void);

// Allocate storage for data payload
SspData* SSPCOM_AllocateSspData(UINT16 dataSize);

// Deallocate data payload storage
void SSPCOM_DeallocateSspData(SspData* sspData);

// Initialize SspData structure
SspData* SSPCOM_InitSspData(SspData* sspData, UINT16 dataSize);

// Open a socket
SspErr SSPCOM_OpenSocket(SspPortId portId, UINT8 socketId);

// Close a socket
SspErr SSPCOM_CloseSocket(UINT8 socketId);

// Get the port ID assisgned to a socket
SspErr SSPCOM_GetPortId(UINT8 socketId, SspPortId* portId);

// Get the port open state
BOOL SSPCOM_IsPortOpen(SspPortId portId);

// Get the socket open state
BOOL SSPCOM_IsSocketOpen(UINT8 socketId);

// Send data over a socket
SspErr SSPCOM_Send(SspData* sspData);

// Flush data on a port
SspErr SSPCOM_Flush(SspPortId portId);

// Process receive data
SspErr SSPCOM_ProcessReceive(SspPortId portId, const SspData** sspData, UINT16 timeout);

#ifdef __cplusplus
}
#endif

#endif // SSP_COM_H
