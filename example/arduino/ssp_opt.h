// SPP build options

#ifndef SSP_OPT_H
#define SSP_OPT_H

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

// Defined OSAL modules
#define SSP_OSAL_NO_OS  1
#define SSP_OSAL_WIN    2
#define SSP_OSAL_STD    3

// Defined HAL modules
#define SSP_HAL_MEM_BUF 1
#define SSP_HAL_WIN     2
#define SSP_HAL_ARDUINO 3

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
#ifdef __GNUC__
#define SSP_OSAL        SSP_OSAL_NO_OS
#define SSP_HAL         SSP_OSAL_NO_OS
#endif

#endif
