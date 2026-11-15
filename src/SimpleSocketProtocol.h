// Simple Socket Protocol (SSP): include this, or ssp.h directly.
// On Arduino it also brings in ssp_arduino.h (SSP_ArduinoSetPort,
// SSP_ArduinoSetTrace).
//
// @see https://github.com/endurodave/SimpleSocketProtocol

#ifndef SIMPLE_SOCKET_PROTOCOL_H
#define SIMPLE_SOCKET_PROTOCOL_H

#include "ssp.h"

#if defined(ARDUINO) && defined(__cplusplus)
#include "ssp_arduino.h"
#endif

#endif // SIMPLE_SOCKET_PROTOCOL_H
