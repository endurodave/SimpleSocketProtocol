// Simple Socket Protocol (SSP) PlatformIO example: two SSP ports talk to each
// other. Socket 0 (port 1) and socket 1 (port 2) exchange messages; each one is
// ACKed by the receiver, and the sender retries until the ACK arrives.
//
// Arduino: port 1 is Serial1 and port 2 is Serial2, so connect them with two
// jumper wires (Mega 2560: pin 18 TX1 -> pin 17 RX2, pin 16 TX2 -> pin 19 RX1).
// Without the wires every send times out and fails after its retries. The
// *_loopback environments instead loop the ports back in memory
// (-DSSP_HAL=SSP_HAL_MEM_BUF): the whole protocol runs, without the UARTs.
// Desktop (native): always loops back in memory.
//
// Log output goes to Serial (USB) at 115200.
//
// @see https://github.com/endurodave/SimpleSocketProtocol

#include "ssp.h"
#include <stdio.h>
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

static const int ROUNDS = 10;

// Per socket: messages received, sends ACKed, sends failed
static int received[2], acked[2], failed[2];

static void Log(const char* msg)
{
#ifdef ARDUINO
    Serial.println(msg);
#else
    printf("%s\n", msg);
#endif
}

static void SspErrorHandler(SspErr err)
{
    char buf[40];
    snprintf(buf, sizeof(buf), "SSP error %d", (int)err);
    Log(buf);
}

// One callback for both sockets: SSP_RECEIVE for incoming data, SSP_SEND when
// an outgoing message was ACKed (SSP_SUCCESS) or gave up after retries
static void SocketCallback(UINT8 socketId, const void* data, UINT16 dataSize,
    SspDataType type, SspErr status, void* userData)
{
    (void)dataSize;
    (void)userData;
    char buf[64];
    if (type == SSP_RECEIVE && status == SSP_SUCCESS)
    {
        received[socketId]++;
        snprintf(buf, sizeof(buf), "socket %u received \"%s\"", socketId, (const char*)data);
        Log(buf);
    }
    else if (type == SSP_SEND)
    {
        if (status == SSP_SUCCESS)
            acked[socketId]++;
        else
            failed[socketId]++;
    }
}

static bool Run()
{
    char buf[80];

    SSP_SetErrorHandler(SspErrorHandler);
    SSP_Init(SSP_PORT1);
    SSP_Init(SSP_PORT2);
    SSP_OpenSocket(SSP_PORT1, 0);
    SSP_OpenSocket(SSP_PORT2, 1);
    SSP_Listen(0, &SocketCallback, NULL);
    SSP_Listen(1, &SocketCallback, NULL);

    for (int round = 1; round <= ROUNDS; round++)
    {
        char msg[24];
        snprintf(msg, sizeof(msg), "from port 1 #%d", round);
        SSP_Send(0, 1, msg, (UINT16)(strlen(msg) + 1));     // socket 0 -> socket 1
        snprintf(msg, sizeof(msg), "from port 2 #%d", round);
        SSP_Send(1, 0, msg, (UINT16)(strlen(msg) + 1));     // socket 1 -> socket 0

        // Run SSP until everything is sent and ACKed (or has failed) and received
        do
        {
            SSP_Process();
        } while (!SSP_IsRecvQueueEmpty(SSP_PORT1) || !SSP_IsRecvQueueEmpty(SSP_PORT2) ||
                 SSP_GetSendQueueSize(SSP_PORT1) != 0 || SSP_GetSendQueueSize(SSP_PORT2) != 0);
    }

    snprintf(buf, sizeof(buf), "socket 0: %d received, %d ACKed, %d failed", received[0], acked[0], failed[0]);
    Log(buf);
    snprintf(buf, sizeof(buf), "socket 1: %d received, %d ACKed, %d failed", received[1], acked[1], failed[1]);
    Log(buf);

    bool ok = received[0] == ROUNDS && received[1] == ROUNDS &&
              acked[0] == ROUNDS && acked[1] == ROUNDS && failed[0] == 0 && failed[1] == 0;
#if defined(ARDUINO) && (SSP_HAL == SSP_HAL_ARDUINO)
    if (!ok)
        Log("No replies: are Serial1 and Serial2 wired together? (see the top of main.cpp)");
#endif
    Log(ok ? "SSP demo PASSED" : "SSP demo FAILED");
    return ok;
}

#ifdef ARDUINO
void setup()
{
    Serial.begin(115200);
    delay(1000);
    Log("SSP demo");
    Run();
}

void loop()
{
}
#else
int main()
{
    Log("SSP demo");
    bool ok = Run();
    SSP_Term();
    return ok ? 0 : 1;
}
#endif
