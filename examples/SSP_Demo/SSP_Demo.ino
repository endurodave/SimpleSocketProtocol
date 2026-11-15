// Simple Socket Protocol (SSP) demo: two SSP ports on one board talk to each
// other. Socket 0 (port 1, Serial1) and socket 1 (port 2, Serial2) exchange
// messages; each one is ACKed by the receiver, and the sender retries until
// the ACK arrives.
//
// Wiring (Mega 2560 or Due): TX1 pin 18 -> RX2 pin 17, TX2 pin 16 -> RX1 pin 19.
// Without the wires every send times out and fails after its retries.
// Results are printed on Serial (the USB port) at 115200.
//
// @see https://github.com/endurodave/SimpleSocketProtocol

#include <ssp.h>

static const int ROUNDS = 10;

// Per socket: messages received, sends ACKed, sends failed
static int received[2], acked[2], failed[2];

// One callback for both sockets: SSP_RECEIVE for incoming data, SSP_SEND when
// an outgoing message was ACKed (SSP_SUCCESS) or gave up after retries
static void SocketCallback(UINT8 socketId, const void* data, UINT16 dataSize,
    SspDataType type, SspErr status, void* userData)
{
    (void)dataSize;
    (void)userData;
    if (type == SSP_RECEIVE && status == SSP_SUCCESS)
    {
        received[socketId]++;
        Serial.print("socket ");
        Serial.print(socketId);
        Serial.print(" received \"");
        Serial.print((const char*)data);
        Serial.println("\"");
    }
    else if (type == SSP_SEND)
    {
        if (status == SSP_SUCCESS)
            acked[socketId]++;
        else
            failed[socketId]++;
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("SSP demo");

    SSP_Init(SSP_PORT1);                    // Serial1
    SSP_Init(SSP_PORT2);                    // Serial2
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

    bool ok = received[0] == ROUNDS && received[1] == ROUNDS &&
              acked[0] == ROUNDS && acked[1] == ROUNDS && failed[0] == 0 && failed[1] == 0;
    if (!ok)
        Serial.println("No replies: are Serial1 and Serial2 wired together?");
    Serial.println(ok ? "SSP demo PASSED" : "SSP demo FAILED");
}

void loop()
{
}
