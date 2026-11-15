// SSP PC host: talks Simple Socket Protocol over a serial port to a board
// running examples/platformio/ssp-device, using the same SSP library as the
// board. Connects, asks the board for its info, then sends numbered messages
// and checks that each is ACKed and echoed back.
//
//   ssp-pc-host <port> [count]
//   ssp-pc-host COM6              Windows
//   ssp-pc-host /dev/ttyACM0 100  Linux
//
// Exit status 0 if every message came back, 1 otherwise.
//
//   ssp-pc-host --device <port> [seconds]
//
// Answers like ssp-device instead, so two PCs (or two ends of a virtual serial
// pair, e.g. socat on Linux) can test the link without a board.
//
// @see https://github.com/endurodave/SimpleSocketProtocol

#include "ssp.h"
#include "ssp_hal_serial.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

using Clock = std::chrono::steady_clock;

static const UINT8 SSP_SOCKET_ID = 0;

// Latest reply from the board and the outcome of the latest send
static std::string reply;
static bool replyReady = false;
static enum { SEND_PENDING, SEND_ACKED, SEND_FAILED } sendState;

static void SocketCallback(UINT8 socketId, const void* data, UINT16 dataSize,
    SspDataType type, SspErr status, void* userData)
{
    (void)socketId;
    (void)userData;
    if (type == SSP_RECEIVE && status == SSP_SUCCESS)
    {
        reply.assign((const char*)data, strnlen((const char*)data, dataSize));
        replyReady = true;
    }
    else if (type == SSP_SEND)
        sendState = status == SSP_SUCCESS ? SEND_ACKED : SEND_FAILED;
}

// Run SSP for up to 'ms', stopping early once 'done' returns true
template <typename Done>
static bool RunUntil(int ms, Done done)
{
    auto end = Clock::now() + std::chrono::milliseconds(ms);
    while (Clock::now() < end)
    {
        SSP_Process();
        if (done())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return done();
}

// Send a request and wait for its ACK and the board's reply
static bool Request(const std::string& msg, int timeoutMs)
{
    replyReady = false;
    sendState = SEND_PENDING;
    if (SSP_Send(SSP_SOCKET_ID, SSP_SOCKET_ID, msg.c_str(), (UINT16)(msg.size() + 1)) != SSP_SUCCESS)
        return false;
    if (!RunUntil(timeoutMs, [] { return sendState != SEND_PENDING; }) || sendState != SEND_ACKED)
        return false;
    return RunUntil(timeoutMs, [] { return replyReady; });
}

// Device mode: answer requests like examples/platformio/ssp-device
static int RunDevice(int seconds)
{
    unsigned long rxCount = 0;
    auto start = Clock::now();
    auto end = start + std::chrono::seconds(seconds);
    printf("Answering requests for %d s\n", seconds);
    while (Clock::now() < end)
    {
        if (!RunUntil(100, [] { return replyReady; }))
            continue;
        replyReady = false;
        rxCount++;
        std::string answer;
        if (reply == "info")
        {
            auto up = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - start).count();
            answer = "info pc-host up " + std::to_string(up) + "s rx " + std::to_string(rxCount);
        }
        else
            answer = "echo: " + reply;
        SSP_Send(SSP_SOCKET_ID, SSP_SOCKET_ID, answer.c_str(), (UINT16)(answer.size() + 1));
    }
    printf("Answered %lu requests\n", rxCount);
    return 0;
}

int main(int argc, char* argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);       // show output at once, even if piped
    bool device = argc > 1 && strcmp(argv[1], "--device") == 0;
    int first = device ? 2 : 1;     // index of the port argument
    if (argc <= first)
    {
        printf("usage: %s <serial port> [count]   e.g. COM6 or /dev/ttyACM0\n"
               "       %s --device <serial port> [seconds]\n", argv[0], argv[0]);
        return 2;
    }
    const char* port = argv[first];
    int count = argc > first + 1 ? atoi(argv[first + 1]) : (device ? 60 : 20);

    SSPHAL_SetPortName(SSP_PORT1, port);
    if (SSP_Init(SSP_PORT1) != SSP_SUCCESS)
    {
        printf("Can't open %s\n", port);
        return 1;
    }
    SSP_OpenSocket(SSP_PORT1, SSP_SOCKET_ID);
    SSP_Listen(SSP_SOCKET_ID, &SocketCallback, NULL);
    printf("Opened %s\n", port);

    if (device)
    {
        int status = RunDevice(count);
        SSP_Term();
        return status;
    }

    // Connect: some boards (e.g. Arduino Mega) reset when the port opens and
    // take a couple of seconds to start, so keep asking for up to 10 s
    bool connected = false;
    for (auto end = Clock::now() + std::chrono::seconds(10); Clock::now() < end && !connected;)
        connected = Request("info", 1500);
    if (!connected)
    {
        printf("No answer from the board on %s. Is it running ssp-device?\n", port);
        SSP_Term();
        return 1;
    }
    printf("Board: %s\n", reply.c_str());

    // Echo test
    int ok = 0;
    double totalMs = 0, maxMs = 0;
    for (int i = 1; i <= count; i++)
    {
        std::string msg = "message " + std::to_string(i);
        auto start = Clock::now();
        bool good = Request(msg, 2000) && reply == "echo: " + msg;
        double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        if (good)
        {
            ok++;
            totalMs += ms;
            if (ms > maxMs)
                maxMs = ms;
        }
        else
            printf("  %s: %s\n", msg.c_str(), replyReady ? ("wrong reply \"" + reply + "\"").c_str() : "no reply");
    }

    if (Request("info", 1500))
        printf("Board: %s\n", reply.c_str());
    if (ok > 0)
        printf("%d/%d echoed, round trip avg %.1f ms, max %.1f ms\n", ok, count, totalMs / ok, maxMs);
    else
        printf("0/%d echoed\n", count);

    bool passed = ok == count;
    printf("%s\n", passed ? "SSP PC host PASSED" : "SSP PC host FAILED");
    SSP_Term();
    return passed ? 0 : 1;
}
