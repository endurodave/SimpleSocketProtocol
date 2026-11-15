# SSP Arduino IDE sketch

`arduino.ino` sends messages between SSP port 1 (`Serial1`) and port 2 (`Serial2`), so on a Mega 2560 or Due connect them with two jumper wires: TX1 pin 18 to RX2 pin 17, TX2 pin 16 to RX1 pin 19. Log output is on `Serial` at 9600 baud.

The Arduino IDE only builds files inside the sketch folder, so the SSP sources are copied in, not kept here. Before opening the sketch, run once (and again after updating the repository):

- Windows: `copysrc.bat`
- Linux/macOS: `sh copysrc.sh`

The copied files are ignored by git.

With PlatformIO none of this is needed: see the `ssp-demo` and `ssp-device` examples in `examples/platformio`, and the PlatformIO section of the main README.

https://github.com/endurodave/SimpleSocketProtocol
