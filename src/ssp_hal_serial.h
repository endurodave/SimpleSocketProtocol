// Serial port selection for the PC serial HALs: SSP_HAL_WIN (Windows COM port)
// and SSP_HAL_SERIAL_POSIX (Linux tty). Call before SSP_Init().

#ifndef SSP_HAL_SERIAL_H
#define SSP_HAL_SERIAL_H

#include "ssp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Set the serial device for an SSP port, e.g. "COM3" on Windows or
/// "/dev/ttyUSB0" on Linux. Defaults: COM1/COM2, /dev/ttyUSB0 and /dev/ttyUSB1.
void SSPHAL_SetPortName(SspPortId portId, const char* name);

/// Set the baud rate for all ports (default 115200). Call before SSP_Init().
void SSPHAL_SetBaudRate(UINT32 baudRate);

#ifdef __cplusplus
}
#endif

#endif // SSP_HAL_SERIAL_H
