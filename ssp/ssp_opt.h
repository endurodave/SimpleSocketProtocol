// SPP build options

#ifndef SSP_OPT_H
#define SSP_OPT_H

// Defined OSAL modules
#define SSP_OSAL_NO_OS  1       // No operating system
#define SSP_OSAL_WIN    2       // Windows operating system
#define SSP_OSAL_STD    3       // C++ standard library
#define SSP_OSAL_UNIX    4      // Linux/Unix operating system

// Defined HAL modules
#define SSP_HAL_MEM_BUF 1       // Memory buffers simulate communication
#define SSP_HAL_WIN     2       // Windows serial communication
#define SSP_HAL_ARDUINO 3       // Arduino serial communication
#define SSP_HAL_LOCALHOST 4     // Linux localhost communication

// Users can override ssp_opt.h with their own configuration by defining
// SSP_CONFIG as a header file to include (-DSSP_CONFIG=ssp_opt_cus.h).
#ifdef SSP_CONFIG
#define SSP_STRINGIZE(x) SSP_STRINGIZE2(x)
#define SSP_STRINGIZE2(x) #x
#include SSP_STRINGIZE(SSP_CONFIG)
#else
// How long to wait for remote CPU to provide and ACK or NAK
#define SSP_ACK_TIMEOUT     200    // in mS

// How many times to retry a failed message
#define SSP_MAX_RETRIES     4

// How long to wait for a incoming character polling com port
#define SSP_RECV_TIMEOUT    10  // in mS

// Maximum number of outgoing messages per port that can be queued
#define SSP_MAX_MESSAGES    5

// Maximum packet size including header, body and CRC (max value 256)
#define SSP_MAX_PACKET_SIZE 64

// Define to output log messages
#define USE_SSP_TRACE

// Define uses fixed block allocator. Undefine uses malloc/free. 
#define USE_FB_ALLOCATOR

// Arduino build options
#ifdef ARDUINO
#define SSP_OSAL        SSP_OSAL_NO_OS
#define SSP_HAL         SSP_HAL_ARDUINO
#endif

// Windows build options
#ifdef WIN32
//#define SSP_OSAL        SSP_OSAL_NO_OS
//#define SSP_OSAL        SSP_OSAL_WIN
#define SSP_OSAL        SSP_OSAL_STD
#define SSP_HAL         SSP_HAL_MEM_BUF
//#define SSP_HAL         SSP_HAL_WIN
#endif

// GCC build options
#ifdef BARE_METAL
#define SSP_OSAL        SSP_OSAL_NO_OS
#define SSP_HAL         SSP_HAL_MEM_BUF
#endif

// GCC build options
#if defined __unix__
#define SSP_OSAL        SSP_OSAL_UNIX
#define SSP_HAL         SSP_HAL_MEM_BUF
//#define SSP_HAL         SSP_HAL_LOCALHOST
#endif

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

#endif	//#ifdef SSP_CONFIG
#endif	//#ifndef SSP_OPT_H

