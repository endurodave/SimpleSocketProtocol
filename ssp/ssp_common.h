// SSP common public header. Minimize file include dependencies. 

#ifndef SSP_COMMON_H
#define SSP_COMMON_H

#include "ssp_types.h"
#include "ssp_opt.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    SSP_RECEIVE,
    SSP_SEND
} SspDataType;

typedef enum
{
    SSP_SUCCESS,
    SSP_BAD_SIGNATURE,
    SSP_PARTIAL_PACKET,
    SSP_PARTIAL_PACKET_HEADER_VALID,
    SSP_PORT_OPEN_FAILED,
    SSP_SOCKET_NOT_OPEN,
    SSP_PORT_NOT_OPEN,
    SSP_BAD_SOCKET_ID,
    SSP_SOCKET_ALREADY_OPEN,
    SSP_PACKET_TOO_LARGE,
    SSP_DATA_SIZE_TOO_LARGE,
    SSP_PARSE_ERROR,
    SSP_CORRUPTED_PACKET,
    SSP_BAD_HEADER_CHECKSUM,
    SSP_SEND_RETRIES_FAILED,
    SSP_QUEUE_FULL,
    SSP_OUT_OF_MEMORY,
    SSP_BAD_ARGUMENT,
    SSP_SEND_FAILURE,
    SSP_NOT_INITIALIZED,
    SSP_DUPLICATE_LISTENER,
    SSP_SOFTWARE_FAULT
} SspErr;

typedef enum
{
    SSP_INVALID_PORT = 0,	// Must be 0
    SSP_PORT1 = 1,          /// @TODO: Define port ID's 
    SSP_PORT2 = 2,
    SSP_MAX_PORTS
} SspPortId;

typedef enum
{
    SSP_SOCKET_MIN = 0,	    // Must be 0
    SSP_SOCKET_COMMAND = SSP_SOCKET_MIN, /// @TODO: Define socket ID's
    SSP_SOCKET_STATUS,
    SSP_SOCKET_LOG,  
    SSP_SOCKET_MAX
} SspSocketId;

// Error handler callback function signature
typedef void(*ErrorHandler)(SspErr err);

void SSP_TraceFormat(const char* format, ...);
void SSP_Trace(const char* str);

#ifdef USE_SSP_TRACE
#define SSP_TRACE_FORMAT(f, ...) SSP_TraceFormat(f, __VA_ARGS__)
#define SSP_TRACE(s) SSP_Trace(s)
#else
#define SSP_TRACE_FORMAT(f, ...)
#define SSP_TRACE(m)
#endif

#ifdef __cplusplus
}
#endif

#endif // SSP_COMMON_H
