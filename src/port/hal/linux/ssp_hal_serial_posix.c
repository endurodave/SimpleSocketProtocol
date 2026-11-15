// Implements the SSP HAL (hardware abstraction layer) interface within ssp_hal.h
// for a serial port on Linux (POSIX termios). Select the device with
// SSPHAL_SetPortName() (ssp_hal_serial.h) before SSP_Init(); defaults are
// /dev/ttyUSB0 and /dev/ttyUSB1. Raw 8N1, no flow control.

#include "ssp_opt.h"

#if (SSP_HAL == SSP_HAL_SERIAL_POSIX)

#include "ssp_hal.h"
#include "ssp_hal_serial.h"
#include "ssp_osal.h"
#include "ssp_fault.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define SSP_LOCK_WAIT_DEFAULT   5000
#define PORT_NAME_MAX           64

static BOOL powerSave = TRUE;
static BOOL initDone = FALSE;
static int m_fd[SSP_MAX_PORTS];
static SSP_OSAL_HANDLE hSerialLock[SSP_MAX_PORTS];
static UINT32 m_baudRate = 115200;
static char m_portName[SSP_MAX_PORTS][PORT_NAME_MAX] = { "", "/dev/ttyUSB0", "/dev/ttyUSB1" };

void SSPHAL_SetPortName(SspPortId portId, const char* name)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS && name != NULL);
    strncpy(m_portName[portId], name, PORT_NAME_MAX - 1);
    m_portName[portId][PORT_NAME_MAX - 1] = 0;
}

void SSPHAL_SetBaudRate(UINT32 baudRate)
{
    m_baudRate = baudRate;
}

static speed_t ToSpeed(UINT32 baud)
{
    switch (baud)
    {
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 230400: return B230400;
    default: return B115200;
    }
}

//----------------------------------------------------------------------------
// ConfigureComPort: raw 8N1, no flow control, reads return what is available
//----------------------------------------------------------------------------
static BOOL ConfigureComPort(int fd)
{
    struct termios tio;
    if (tcgetattr(fd, &tio) != 0)
        return FALSE;

    cfmakeraw(&tio);
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~(CSTOPB | PARENB | CRTSCTS);
    tio.c_iflag &= ~(IXON | IXOFF | IXANY);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    cfsetispeed(&tio, ToSpeed(m_baudRate));
    cfsetospeed(&tio, ToSpeed(m_baudRate));

    if (tcsetattr(fd, TCSANOW, &tio) != 0)
        return FALSE;
    tcflush(fd, TCIOFLUSH);
    return TRUE;
}

void SSPHAL_Init(SspPortId portId)
{
    (void)portId;
    // SSP calls this once per port: set up all ports on the first call only
    if (initDone)
        return;
    initDone = TRUE;

    for (INT port = 0; port < SSP_MAX_PORTS; port++)
    {
        m_fd[port] = -1;
        hSerialLock[port] = SSPOSAL_LockCreate();
        ASSERT_TRUE(hSerialLock[port] != SSP_OSAL_INVALID_HANDLE_VALUE);
    }
}

void SSPHAL_Term(void)
{
    if (!initDone)
        return;

    for (INT port = SSP_PORT1; port < SSP_MAX_PORTS; port++)
        SSPHAL_PortClose((SspPortId)port);
    for (INT port = 0; port < SSP_MAX_PORTS; port++)
    {
        SSPOSAL_LockDestroy(hSerialLock[port]);
        hSerialLock[port] = SSP_OSAL_INVALID_HANDLE_VALUE;
    }
    initDone = FALSE;
}

BOOL SSPHAL_PortOpen(SspPortId portId)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);

    if (SSPHAL_PortIsOpen(portId) == TRUE)
        return TRUE;

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    BOOL success = FALSE;
    int fd = open(m_portName[portId], O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd >= 0)
    {
        if (ConfigureComPort(fd))
        {
            m_fd[portId] = fd;
            success = TRUE;
        }
        else
            close(fd);
    }

    SSPOSAL_LockPut(hSerialLock[portId]);

    // A missing or busy port is reported to the caller (SSP_PORT_OPEN_FAILED)
    return success;
}

void SSPHAL_PortClose(SspPortId portId)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    if (m_fd[portId] >= 0)
    {
        close(m_fd[portId]);
        m_fd[portId] = -1;
    }

    SSPOSAL_LockPut(hSerialLock[portId]);
}

BOOL SSPHAL_PortIsOpen(SspPortId portId)
{
    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);
    BOOL isOpen = m_fd[portId] >= 0 ? TRUE : FALSE;
    SSPOSAL_LockPut(hSerialLock[portId]);
    return isOpen;
}

BOOL SSPHAL_PortSend(SspPortId portId, const char* buf, UINT16 bytesToSend)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);

    if (NULL == buf || 0 == bytesToSend)
        return FALSE;

    BOOL success = FALSE;
    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    int fd = m_fd[portId];
    if (fd >= 0)
    {
        // The port is non-blocking: keep writing until all bytes are queued
        UINT16 sent = 0;
        while (sent < bytesToSend)
        {
            ssize_t n = write(fd, buf + sent, bytesToSend - sent);
            if (n > 0)
                sent += (UINT16)n;
            else if (n < 0 && (errno == EAGAIN || errno == EINTR))
            {
                struct pollfd pfd = { fd, POLLOUT, 0 };
                poll(&pfd, 1, 100);
            }
            else
                break;
        }
        success = sent == bytesToSend ? TRUE : FALSE;
    }

    SSPOSAL_LockPut(hSerialLock[portId]);
    return success;
}

BOOL SSPHAL_PortRecv(SspPortId portId, char* buf, UINT16* bytesRead, UINT16 maxLen, UINT16 timeout)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);

    if (NULL == buf || NULL == bytesRead)
        return FALSE;

    *bytesRead = 0;
    if (maxLen == 0)
        return FALSE;

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    BOOL success = FALSE;
    int fd = m_fd[portId];
    if (fd >= 0)
    {
        // Wait up to timeout for data, then read what has arrived
        struct pollfd pfd = { fd, POLLIN, 0 };
        int ready = poll(&pfd, 1, timeout);
        if (ready > 0)
        {
            ssize_t n = read(fd, buf, maxLen);
            if (n >= 0)
            {
                *bytesRead = (UINT16)n;
                success = TRUE;
            }
        }
        else if (ready == 0)
            success = TRUE;     // timeout, nothing read
    }

    SSPOSAL_LockPut(hSerialLock[portId]);
    return success;
}

BOOL SSPHAL_IsRecvQueueEmpty(SspPortId portId)
{
    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);

    int available = 0;
    if (m_fd[portId] >= 0 && ioctl(m_fd[portId], FIONREAD, &available) != 0)
        available = 0;

    SSPOSAL_LockPut(hSerialLock[portId]);
    return available > 0 ? FALSE : TRUE;
}

void SSPHAL_PortFlush(SspPortId portId)
{
    ASSERT_TRUE(portId >= SSP_PORT1 && portId < SSP_MAX_PORTS);

    SSPOSAL_LockGet(hSerialLock[portId], SSP_LOCK_WAIT_DEFAULT);
    if (m_fd[portId] >= 0)
        tcflush(m_fd[portId], TCIOFLUSH);
    SSPOSAL_LockPut(hSerialLock[portId]);
}

void SSPHAL_PowerSave(BOOL enable)
{
    powerSave = enable;
}

BOOL SSPHAL_IsPowerSave(void)
{
    return powerSave;
}

#endif
