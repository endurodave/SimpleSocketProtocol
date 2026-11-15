// SSP Arduino options, set at run time (the Arduino IDE has no build flags).
// Call before SSP_Init(). C++ only.
//
//   Serial.begin(115200);
//   SSP_ArduinoSetPort(SSP_PORT1, Serial);   // SSP over the USB serial port
//   SSP_ArduinoSetTrace(nullptr);            // no trace text on that port
//   SSP_Init(SSP_PORT1);
//
// @see https://github.com/endurodave/SimpleSocketProtocol

#ifndef SSP_ARDUINO_H
#define SSP_ARDUINO_H

#ifdef ARDUINO

#include <Arduino.h>
#include "ssp_common.h"

/// Use 'stream' for an SSP port instead of the default (Serial1 for port 1,
/// Serial2 for port 2, where the board has them). The sketch owns the stream:
/// call begin() on it yourself; SSP only reads and writes it.
void SSP_ArduinoSetPort(SspPortId portId, Stream& stream);

/// Where SSP trace text goes (default Serial). nullptr turns trace off, which
/// is required when Serial is also an SSP port.
void SSP_ArduinoSetTrace(Print* out);

#endif // ARDUINO

#endif // SSP_ARDUINO_H
