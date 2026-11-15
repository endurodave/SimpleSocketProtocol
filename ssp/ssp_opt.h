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
#define SSP_HAL_SERIAL_POSIX 5  // Linux serial port (termios)

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

// Define to output log messages (build with -DSSP_NO_TRACE to turn them off,
// e.g. when the log would go out on the same serial port as SSP)
#ifndef SSP_NO_TRACE
#define USE_SSP_TRACE
#endif

// Define uses fixed block allocator. Undefine uses malloc/free.
#define USE_FB_ALLOCATOR

// Arduino build options. Override either with a build flag, e.g.
// -DSSP_HAL=SSP_HAL_MEM_BUF to loop ports 1 and 2 back in memory (no wiring).
#ifdef ARDUINO
#ifndef SSP_OSAL
#define SSP_OSAL        SSP_OSAL_NO_OS
#endif
#ifndef SSP_HAL
#define SSP_HAL         SSP_HAL_ARDUINO
#endif
#endif

// Windows build options (OSAL: SSP_OSAL_STD or SSP_OSAL_WIN; HAL: SSP_HAL_MEM_BUF
// or SSP_HAL_WIN for a COM port). Override with build flags.
#ifdef WIN32
#ifndef SSP_OSAL
#define SSP_OSAL        SSP_OSAL_STD
#endif
#ifndef SSP_HAL
#define SSP_HAL         SSP_HAL_MEM_BUF
#endif
#endif

// GCC build options
#ifdef BARE_METAL
#ifndef SSP_OSAL
#define SSP_OSAL        SSP_OSAL_NO_OS
#endif
#ifndef SSP_HAL
#define SSP_HAL         SSP_HAL_MEM_BUF
#endif
#endif

// Linux/Unix build options (HAL: SSP_HAL_MEM_BUF, SSP_HAL_LOCALHOST or
// SSP_HAL_SERIAL_POSIX for a serial port). Override with build flags.
#if defined __unix__
#ifndef SSP_OSAL
#define SSP_OSAL        SSP_OSAL_UNIX
#endif
#ifndef SSP_HAL
#define SSP_HAL         SSP_HAL_MEM_BUF
#endif
#endif

typedef enum
{
    SSP_INVALID_PORT = 0,   // Must be available and must be 0
    SSP_PORT1 = 1,          // PORT1 (may be replaced....), must be 1
    SSP_PORT2 = 2,          // PORT2 (may be replaced....)
    SSP_MAX_PORTS           // Must be available and last no.
} SspPortId;                // Enum OBLIGATORY!

typedef enum
{
    SSP_SOCKET_MIN = 0,	    // Must be available and must be 0
    SSP_SOCKET_COMMAND = SSP_SOCKET_MIN, /// @TODO: Define socket ID's
    SSP_SOCKET_STATUS,
    SSP_SOCKET_LOG,
    SSP_SOCKET_MAX          // Must be available and last no.
} SspSocketId;              // Enum OBLIGATORY!

#endif	//#ifdef SSP_CONFIG
#endif	//#ifndef SSP_OPT_H

