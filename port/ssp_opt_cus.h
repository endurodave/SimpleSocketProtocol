// SPP build options
#ifndef SSP_OPT_CUS_H
#define SSP_OPT_CUS_H


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
//#define USE_SSP_TRACE

// Define uses fixed block allocator. Undefine uses malloc/free. 
#define USE_FB_ALLOCATOR

// Maximum number of bytes to read from communication port on each
// call to SSPHAL_PortRecv(), otherwise in stack this is 1
// If communication port driver guarantees one full SSP message when
// SSPHAL_PortRecv() called (like maybe a DMA SPI driver)
#define MAX_PORT_RECV_BYTES     SSP_PACKET_SIZE(SSP_MAX_BODY_SIZE)

// Windows build options
#ifdef WIN32
//#define SSP_OSAL        SSP_OSAL_NO_OS
//#define SSP_OSAL        SSP_OSAL_WIN
#define SSP_OSAL        SSP_OSAL_STD
#define SSP_HAL         SSP_HAL_MEM_BUF
//#define SSP_HAL         SSP_HAL_WIN
#endif

// GCC build options
#if defined __unix__
#define SSP_OSAL        SSP_OSAL_UNIX
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

#endif	//#ifndef SSP_OPT_CUS_H
