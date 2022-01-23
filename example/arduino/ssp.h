// Simple Socket Protocol (SSP) main interface used by the application layer to 
// send/receive data. Application only includes ssp.h. All other header interfaces 
// are private. 
//
// @see https://www.codeproject.com/Articles/5321271/Simple-Socket-Protocol-for-Embedded-Systems
// David Lafreniere, Jan 2022.

#ifndef SSP_H
#define SSP_H

#include "ssp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/// SSP callback function signature for receiving socket data asynchronously.
/// Called upon transmission success or failure. 
/// @param[in] socketId The socket identifier.
/// @param[in] data The incoming data if type is SSP_RECEIVE. The outgoing data
///     if type is SSP_SEND.
/// @param[in] dataSize The number of bytes pointed to by data.
/// @param[in] type The type of callback: SSP_RECEIVE or SSP_SEND.
/// @param[in] status The status of the send/receive.
/// @param[in] userData Optional user data pointer that was provided in 
///     SSP_Listen() callback registration. 
typedef void(*SspDataCallback)(UINT8 socketId, const void* data, UINT16 dataSize,
    SspDataType type, SspErr status, void* userData);

// Called once per port to initialize
SspErr SSP_Init(SspPortId portId);

// Called once when application terminates
void SSP_Term(void);

// Open a socket on a specified port
SspErr SSP_OpenSocket(SspPortId port, UINT8 socketId);

// Close a socket
SspErr SSP_CloseSocket(UINT8 socketId);

// Send data over a socket
SspErr SSP_Send(UINT8 srcSocketId, UINT8 destSocketId, const void* data, UINT16 dataSize);

// Send multiple data arrays over a socket
SspErr SSP_SendMultiple(UINT8 srcSocketId, UINT8 destSocketId, INT16 numData,
    void const** dataArray, UINT16* dataSizeArray);

// Listen for incoming data on a socket using a callback function
SspErr SSP_Listen(UINT8 socketId, SspDataCallback callback, void* userData);

// Get number of pending messages in outgoing queue
UINT16 SSP_GetSendQueueSize(SspPortId portId);

// Determine if the incoming queue has data or not
BOOL SSP_IsRecvQueueEmpty(SspPortId portId);

// Process outgoing and incoming socket messages
void SSP_Process(void);

// Register for callbacks when SSP error occurs
void SSP_SetErrorHandler(ErrorHandler handler);

// Get the last SSP error
SspErr SSP_GetLastErr(void);

#ifdef __cplusplus
}
#endif

#endif // SSP_H
