// Test SSP library using serialize class to binary encode payload data.
// A simple serialize class to binary serialize and deserialize C++ objects.
//
// @see https://github.com/endurodave/MessageSerialize
// @see https://github.com/endurodave/SimpleSocketProtocol
// David Lafreniere, Jan 2024.

#include "ssp.h"
#include "serialize.h"
#include <stdio.h>
#include <string.h>
#include "ssp_fault.h"
#include <sstream>
#include <vector>

using namespace std;

// Measurement data with serialize
class Measurement : public serialize::I
{
public:
    Measurement(float v1, float v2) : value1(v1), value2(v2) {}
    Measurement() = default;

    virtual ostream& write(serialize& ms, ostream& os) override
    {
        ms.write(os, value1);
        ms.write(os, value2);
        return os;
    }

    virtual istream& read(serialize& ms, istream& is) override
    {
        ms.read(is, value1);
        ms.read(is, value2);
        return is;
    }

    float value1 = 0.0f;
    float value2 = 0.0f;
};

// Message data with serialize
class Message : public serialize::I
{
public:
    virtual ostream& write(serialize& ms, ostream& os) override
    {
        ms.write(os, msg);
        ms.write(os, cnt);
        ms.write(os, data);
        return os;
    }

    virtual istream& read(serialize& ms, istream& is) override
    {
        ms.read(is, msg);
        ms.read(is, cnt);
        ms.read(is, data);
        return is;
    }

    string msg;
    int16_t cnt = 0;
    vector<Measurement> data;
};

static void SspErrorHandler(SspErr err)
{
    SSP_TRACE_FORMAT("SspErrorHandler: %d\n", err);
}

static void SspCallbackSocket0(UINT8 socketId, const void* data, UINT16 dataSize,
    SspDataType type, SspErr status, void* userData)
{
    // Received data callback?
    if (type == SSP_RECEIVE)
    {
        // Received successfully?
        if (status == SSP_SUCCESS)
        {
            serialize ms;
            Message msg;

            // Convert incoming bytes to a stream for parsing
            istringstream is(std::string((char*)data, dataSize), std::ios::in | std::ios::binary);

            // Parse the incoming serialized binary message
            ms.read(is, msg);
            if (is.good())
            {
                // Parse success! Use the message data.
                SSP_TRACE_FORMAT("SSP_RECEIVE PORT1: %s %d", msg.msg.c_str(), msg.cnt);
            }
        }
    }
    // Send data callback?
    else if (type == SSP_SEND)
    {
        if (status == SSP_SUCCESS)
            SSP_TRACE("SSP_SEND PORT1 SUCCESS");
        else
            SSP_TRACE("SSP_SEND PORT1 FAIL");
    }
    else
    {
        SSP_TRACE("UNKNOWN PORT1");
    }
}

static void SspCallbackSocket1(UINT8 socketId, const void* data, UINT16 dataSize,
    SspDataType type, SspErr status, void* userData)
{
    // Received data callback?
    if (type == SSP_RECEIVE)
    {
        // Received successfully?
        if (status == SSP_SUCCESS)
        {
            serialize ms;
            Message msg;

            // Convert incoming bytes to a stream for parsing
            istringstream is(std::string((char*)data, dataSize), std::ios::in | std::ios::binary);

            // Parse the incoming serialized binary message
            ms.read(is, msg);
            if (is.good())
            {
                // Parse success! Use the message data.
                SSP_TRACE_FORMAT("SSP_RECEIVE PORT2: %s %d", msg.msg.c_str(), msg.cnt);
            }
        }
    }
    // Send data callback?
    else if (type == SSP_SEND)
    {
        if (status == SSP_SUCCESS)
            SSP_TRACE("SSP_SEND PORT2 SUCCESS");
        else
            SSP_TRACE("SSP_SEND PORT2 FAIL");
    }
    else
    {
        SSP_TRACE("UNKNOWN PORT2");
    }
}

int serialize_example()
{
    // Register for error callbacks
    SSP_SetErrorHandler(SspErrorHandler);

    // Initialize the ports
    SSP_Init(SSP_PORT1);
    SSP_Init(SSP_PORT2);

    SspErr err;

    // Open two sockets
    err = SSP_OpenSocket(SSP_PORT1, 0);
    err = SSP_OpenSocket(SSP_PORT2, 1);

    // Register for callbacks
    err = SSP_Listen(0, &SspCallbackSocket0, NULL);
    err = SSP_Listen(1, &SspCallbackSocket1, NULL);

    int cntr = 0;

    // Message serializer instance
    serialize ms;

    while (1)
    {
        // Create message to send
        Message msg;
        msg.msg = "Data Sample: ";
        msg.cnt = cntr++;
        msg.data.push_back(Measurement(1.23f, 3.45f));

        // Serialize the message
        stringstream ss(ios::in | ios::out | ios::binary);
        ms.write(ss, msg);

        // Send serialized messaged
        err = SSP_Send(0, 1, ss.str().c_str(), (UINT16)ss.tellp());
        err = SSP_Send(1, 0, ss.str().c_str(), (UINT16)ss.tellp());

        do
        {
            // Call while there is send or receive data to process
            SSP_Process();
        } while (!SSP_IsRecvQueueEmpty(SSP_PORT1) ||
            !SSP_IsRecvQueueEmpty(SSP_PORT2) ||
            SSP_GetSendQueueSize(SSP_PORT1) != 0 ||
            SSP_GetSendQueueSize(SSP_PORT2) != 0);

        if (SSP_GetLastErr() != SSP_SUCCESS)
            break;
    }

    err = SSP_CloseSocket(0);
    err = SSP_CloseSocket(1);
    SSP_Term();

    return 0;
}
