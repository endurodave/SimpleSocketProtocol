// Simple Socket Protocol (SSP) PlatformIO example, device side of a PC link:
// the board runs SSP on one serial port to a PC running examples/pc-host.
//
// Socket 0 receives requests and answers each one:
//   "info"        -> "info <board> up <seconds>s rx <count>"
//   anything else -> "echo: <message>"
// SSP ACKs every message and retries until the other side ACKs, so a dropped
// or corrupted byte costs a retry, not a lost message.
//
// The port carries SSP's binary packets only, so SSP's text trace is off
// (-DSSP_NO_TRACE in platformio.ini) and this sketch never prints to it.
//
// @see https://github.com/endurodave/SimpleSocketProtocol

#include "ssp.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#ifndef DEVICE_NAME
#define DEVICE_NAME "arduino"
#endif

static const UINT8 SSP_SOCKET_ID = 0;

// A request received in the callback, answered from loop()
static char request[40];
static volatile bool pending = false;
static unsigned long rxCount = 0;

static void SocketCallback(UINT8 socketId, const void* data, UINT16 dataSize,
    SspDataType type, SspErr status, void* userData)
{
    (void)socketId;
    (void)userData;
    if (type == SSP_RECEIVE && status == SSP_SUCCESS && !pending)
    {
        UINT16 n = dataSize < sizeof(request) - 1 ? dataSize : sizeof(request) - 1;
        memcpy(request, data, n);
        request[n] = 0;
        rxCount++;
        pending = true;
    }
}

void setup()
{
#ifdef LED_BUILTIN
    pinMode(LED_BUILTIN, OUTPUT);
#endif
    SSP_Init(SSP_PORT1);                // opens the serial port (115200)
    SSP_OpenSocket(SSP_PORT1, SSP_SOCKET_ID);
    SSP_Listen(SSP_SOCKET_ID, &SocketCallback, NULL);
}

void loop()
{
    SSP_Process();

    if (pending)
    {
        char reply[56];
        if (strcmp(request, "info") == 0)
            snprintf(reply, sizeof(reply), "info %s up %lus rx %lu",
                DEVICE_NAME, millis() / 1000UL, rxCount);
        else
            snprintf(reply, sizeof(reply), "echo: %s", request);
        pending = false;
        SSP_Send(SSP_SOCKET_ID, SSP_SOCKET_ID, reply, (UINT16)(strlen(reply) + 1));
#ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
#endif
    }
}
