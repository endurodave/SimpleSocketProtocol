#include "ssp.h"
#include "ssp_com.h"
#include "ssp_common_p.h"
#include "ssp_hal.h"
#include "ssp_osal.h"
#include "ssp_util.h"
#include "ssp_fault.h"
#include <string.h>
#ifdef USE_FB_ALLOCATOR
#include "fb_allocator.h"
#else
#include <stdlib.h>
#endif

// Packet types
typedef enum
{
    MSG_TYPE_DATA,
    MSG_TYPE_ACK,
    MSG_TYPE_NAK
} SspMsgType;

typedef enum
{
    SEND_STATE,
    RECEIVE_STATE
} SendDataState;

// Outgoing send data message structure
typedef struct SendData
{
    // Time stamp since the last send
    UINT32 sendTickStamp;

    // How many times has this message retried sending
    UINT32 sendRetries;

    // The current state of this packet transmission
    SendDataState state;

    // SSP data to be transmitted
    SspData* sspData;

    // Pointer to the next structure or NULL if list end
    struct SendData* next;
} SendData;

typedef struct
{
    // Transaction ID
    UINT8 transId;

    // Error check
    UINT16 crc;
} ReceivedTransId;

// Maximum number of SendData memory blocks
#define MAX_SEND_DATA_BLOCKS        SSP_MAX_MESSAGES

#ifdef USE_FB_ALLOCATOR
// Define fixed block allocator and memory for SendData
ALLOC_DEFINE(sendDataAllocator, sizeof(SendData), MAX_SEND_DATA_BLOCKS)
#endif

typedef struct
{
    // A transaction ID that is incremented on each new message
    UINT8 sendTransId;

    // A software lock handle
    SSP_OSAL_HANDLE hSspLock;

    // Socket ID to callback functions array
    SspDataCallback socketToCallbackMap[SSP_SOCKET_MAX];

    // Socket ID to client user data array
    void* socketToUserDataMap[SSP_SOCKET_MAX];

    // Set to TRUE after one time initialization complete
    BOOL initOnce;

    // Linked list head pointers for data to be transmitted grouped by port ID
    SendData* sendDataListHead[SSP_MAX_PORTS];

    // The last received message transaction IDs on each port
    ReceivedTransId lastReceivedTransId[SSP_MAX_PORTS];

    // Dedicated memory for ACK/NAK messages
    UINT8 sspDataForAckNakMem[SSP_DATA_SIZE(0)];

    // Dedicated data structure for ACK/NAK messages
    SspData* sspDataForAckNak;
} SspComObj;

// Private module data
static SspComObj self;

// Private functions
static SendData* AllocSendData(UINT16 dataSize);
static void FreeSendData(SendData* sendData);
static void ListInsert(SspPortId portId, SendData* sendData);
static void ListErase(SspPortId portId, const SendData* sendData);
static SendData* ListFront(SspPortId portId);
static SendData* ListFind(SspPortId portId, const SspPacketHeader* header);
static UINT16 ListSize(SspPortId portId);
static void SendAck(const SspPacketHeader* headerToAck);
static void SendNak(const SspPacketHeader* headerToNak);
static SspDataCallback GetCallbackListener(UINT8 socketId);
static void CallbackListener(UINT8 socketId, const SspData* sspData);
static void NotifyListener(UINT8 socketId, const SspData* sspData);
static void ProcessSend(SspPortId portId);
static void ProcessReceive(SspPortId portId);

/// Allocate a SendData structure
/// @param[in] dataSize The data size of the payload.
/// @return The allocated SendData structure or NULL if fails.
static SendData* AllocSendData(UINT16 dataSize)
{    
#ifdef USE_FB_ALLOCATOR
    SendData* sendData = (SendData*)ALLOC_Calloc(sendDataAllocator, 1, sizeof(SendData));
#else
    SendData* sendData = (SendData*)calloc(1, sizeof(SendData));
#endif
    if (sendData)
    {
        // Allocate a SspData structure to hold the required data size
        sendData->sspData = SSPCOM_AllocateSspData(dataSize);
        if (!sendData->sspData)
        {
            FreeSendData(sendData);
            sendData = NULL;
        }
    }

    return sendData;
}

/// Free a SendData structure
/// @param[in] sendData The previously allocated structure to free.
static void FreeSendData(SendData* sendData)
{
    if (sendData)
    {
        SSPCOM_DeallocateSspData(sendData->sspData);
#ifdef USE_FB_ALLOCATOR
        ALLOC_Free(sendDataAllocator, sendData);
#else
        free(sendData);
#endif
    }
}

/// Insert dynamically allocated SendData instance into a list
/// @param[in] portId A port identifier. 
/// @param[in] sendData The data to insert into the list.
static void ListInsert(SspPortId portId, SendData* sendData)
{
    SendData* msg = NULL;

    ASSERT_TRUE(sendData != NULL);

    SSPOSAL_LockGet(self.hSspLock, SSP_OSAL_WAIT_DEFAULT);

    // Get head of list
    msg = self.sendDataListHead[portId];

    // Head of list NULL?
    if (NULL == msg)
    {
        // Add to head of linked list
        self.sendDataListHead[portId] = sendData;
        sendData->next = NULL;
    }
    else
    {
        // Find list tail
        while (NULL != msg->next)
        {
            // Get next structure
            msg = msg->next;
        }

        // Add message to the end of the list
        msg->next = sendData;
        sendData->next = NULL;
    }

    SSPOSAL_LockPut(self.hSspLock);
} 

/// Remove a SendData instance from the list.
/// @param[in] portId A port identifier. 
/// @param[in] sendData The data item to remove. 
static void ListErase(SspPortId portId, const SendData* sendData)
{
    SendData* currData = NULL;
    SendData* prevData = NULL;

    ASSERT_TRUE(sendData != NULL);

    SSPOSAL_LockGet(self.hSspLock, SSP_OSAL_WAIT_DEFAULT);

    currData = self.sendDataListHead[portId];
    prevData = NULL;
    while (NULL != currData)
    {
        // Is this the SendData to remove from list?
        if (currData->sspData->packet.header.transId == sendData->sspData->packet.header.transId &&
            currData->sspData->packet.header.destId == sendData->sspData->packet.header.destId &&
            currData->sspData->packet.header.srcId == sendData->sspData->packet.header.srcId &&
            currData->sspData->packet.header.type == sendData->sspData->packet.header.type &&
            currData->sspData->packet.header.checksum == sendData->sspData->packet.header.checksum &&
            currData->sspData->packet.header.bodySize == sendData->sspData->packet.header.bodySize)
        {
            // Unlink the current message from the linked list
            if (prevData == NULL)
            {
                self.sendDataListHead[portId] = currData->next;
            }
            else
            {
                prevData->next = currData->next;
            }
            break;
        }
        prevData = currData;
        currData = currData->next;
    }

    SSPOSAL_LockPut(self.hSspLock);
} 

/// Get data at the front of the list. 
/// @param[in] portId A port identifier. 
/// @return A data instance or NULL if list is empty. 
static SendData* ListFront(SspPortId portId)
{
    SendData* data;

    SSPOSAL_LockGet(self.hSspLock, SSP_OSAL_WAIT_DEFAULT);

    // Get the head list element
    data = self.sendDataListHead[portId];

    SSPOSAL_LockPut(self.hSspLock);
    return data;
}

/// Find a SendData instance within the list. 
/// @param[in] portId A port identifier. 
/// @param[in] header A header with data populated to locate the instance.
/// @return A data instance or NULL if not found. 
static SendData* ListFind(SspPortId portId, const SspPacketHeader* header)
{
    SendData* retVal = NULL;
    SendData* msg;

    ASSERT_TRUE(header != NULL);

    SSPOSAL_LockGet(self.hSspLock, SSP_OSAL_WAIT_DEFAULT);

    // Find the SSP message with a matching transaction ID and dest ID
    msg = self.sendDataListHead[portId];
    while (NULL != msg)
    {
        if (msg->sspData->packet.header.destId == header->srcId &&
            msg->sspData->packet.header.srcId == header->destId &&
            msg->sspData->packet.header.transId == header->transId)
        {
            retVal = msg;
            break;
        }
        msg = msg->next;
    }

    SSPOSAL_LockPut(self.hSspLock);
    return retVal;
} 

/// Get the number of elements within the list. 
/// @param[in] portId A port identifier. 
/// @return The number of instances within the list. 
static UINT16 ListSize(SspPortId portId)
{
    UINT16 size = 0;
    SendData* data;

    SSPOSAL_LockGet(self.hSspLock, SSP_OSAL_WAIT_DEFAULT);

    // Count the number of SendData pointers in the list
    data = self.sendDataListHead[portId];
    while (NULL != data)
    {
        size++;
        data = data->next;
    }

    SSPOSAL_LockPut(self.hSspLock);
    return size;
}

/// Send an ACK message.
/// @param[in] headerToAck The header of a message to acknowledge. 
static void SendAck(const SspPacketHeader* headerToAck)
{
    ASSERT_TRUE(headerToAck != NULL);

    if (NULL != self.sspDataForAckNak)
    {
        self.sspDataForAckNak->err = SSP_SUCCESS;
        self.sspDataForAckNak->type = SSP_SEND;
        self.sspDataForAckNak->packet.header.srcId = headerToAck->destId; // dest now src
        self.sspDataForAckNak->packet.header.destId = headerToAck->srcId; // src now dest
        self.sspDataForAckNak->packet.header.bodySize = 0;
        self.sspDataForAckNak->packet.header.transId = headerToAck->transId;  // respond with same transId
        self.sspDataForAckNak->packet.header.type = MSG_TYPE_ACK;

        // Send the ACK message
        SSPCOM_Send(self.sspDataForAckNak);
    }
} 

/// Send an NAK message.
/// @param[in] headerToAck The header of a message to negative acknowledge. 
static void SendNak(const SspPacketHeader* headerToNak)
{
    ASSERT_TRUE(headerToNak != NULL);

    if (NULL != self.sspDataForAckNak)
    {
        self.sspDataForAckNak->err = SSP_SUCCESS;
        self.sspDataForAckNak->type = SSP_SEND;
        self.sspDataForAckNak->packet.header.srcId = headerToNak->destId; // dest now src
        self.sspDataForAckNak->packet.header.destId = headerToNak->srcId; // src now dest
        self.sspDataForAckNak->packet.header.bodySize = 0;
        self.sspDataForAckNak->packet.header.transId = headerToNak->transId;  // respond with same transId
        self.sspDataForAckNak->packet.header.type = MSG_TYPE_NAK;

        // Send the NAK message
        SSPCOM_Send(self.sspDataForAckNak);
    }
} 

/// Get the registered listener callback function
/// @param[in] socketId A socket ID.
/// @return The callback function for the socket ID.
static SspDataCallback GetCallbackListener(UINT8 socketId)
{
    SspDataCallback callback;

    SSPOSAL_LockGet(self.hSspLock, SSP_OSAL_WAIT_DEFAULT);

    // Get the registered callback listener for the socket
    callback = self.socketToCallbackMap[socketId];

    SSPOSAL_LockPut(self.hSspLock);

    return callback;
}

/// Callback the registered listener function pointer. 
/// @param[in] socketId The socket identifier. 
/// @param[in] sspData The data used in the notification. 
static void CallbackListener(UINT8 socketId, const SspData* sspData)
{
    SspDataCallback callback;

    ASSERT_TRUE(sspData != NULL);

    callback = GetCallbackListener(socketId);

    // Is a callback registered?
    if (NULL != callback)
    {
        // Callback client function with the received data
        callback(
                socketId,
                sspData->packet.body,
                sspData->packet.header.bodySize,
                sspData->type,
                sspData->err,
                self.socketToUserDataMap[socketId]);
    }
} 

/// Notify the registered client of reception of data or errors. 
/// @param[in] socketId A socket identifier.
/// @param[in] sspData Data used in the notification. 
static void NotifyListener(UINT8 socketId, const SspData* sspData)
{
    SspPortId portId;
    SspErr err;

    ASSERT_TRUE(sspData != NULL);

    // Client only gets message data packets, not ACK/NAK packets
    if (MSG_TYPE_DATA != sspData->packet.header.type)
        return;

    // Was there a message error?
    if (SSP_SUCCESS != sspData->err)
    {
        // Callback the registered socket listener with the error
        CallbackListener(socketId, sspData);
    }
    else
    {
        // Get the port ID that the incoming message arrived on
        err = SSPCOM_GetPortId(sspData->packet.header.srcId, &portId);
        if (err == SSP_SUCCESS)
        {
            if (sspData->type == SSP_SEND)
            {
                // Send data succeeded. Callback the registered socket listener.
                CallbackListener(socketId, sspData);
            }
            else
            {
                SSPOSAL_LockGet(self.hSspLock, SSP_OSAL_WAIT_DEFAULT);

                // This message already received?
                if (self.lastReceivedTransId[portId].transId == sspData->packet.header.transId &&
                    self.lastReceivedTransId[portId].crc == *sspData->crc)
                {
                    // Duplicate message. Message received already. Do not forward the
                    // message to callback listeners.
                    SSPOSAL_LockPut(self.hSspLock);
                }
                else
                {
                    // New message. Save the transId and crc  received to prevent
                    // duplicate messages from being sent to the callback listeners
                    self.lastReceivedTransId[portId].transId = sspData->packet.header.transId;
                    self.lastReceivedTransId[portId].crc = *sspData->crc;

                    // Release lock before invoking callback below
                    SSPOSAL_LockPut(self.hSspLock);

                    // Receive data succeeded. Callback the registered socket listener.
                    CallbackListener(socketId, sspData);
                }
            }
        }
        else
        {
            // Bad received port ID? Ignore message.
        }
    }
} 

/// Process outgoing socket data to send. 
/// @param[in] portId A port identifier. 
static void ProcessSend(SspPortId portId)
{
    SendData* sendData;
    SspErr err;

    // Get next message to transmit from list front
    sendData = ListFront(portId);

    // If there is data and message is ready to send
    if (NULL != sendData && SEND_STATE == sendData->state)
    {
        // Is retry count low enough to send?
        if (sendData->sendRetries++ <= SSP_MAX_RETRIES)
        {
            // Send the packet
            err = SSPCOM_Send(sendData->sspData);
            if (err == SSP_SUCCESS)
            {
                // Update the time sent
                sendData->sendTickStamp = SSPOSAL_GetTickCount();

                // Waiting for the ACK
                sendData->state = RECEIVE_STATE;
            }
            else
            {
                SSP_TRACE_FORMAT("Send failed. Port: %d Socket: %d Trans: %d",
                    portId, 
                    sendData->sspData->packet.header.srcId, 
                    sendData->sspData->packet.header.transId);
            }
        }
        else
        {
            // Notify client that the retries exceeded
            sendData->sspData->err = SSP_SEND_RETRIES_FAILED;
            NotifyListener(sendData->sspData->packet.header.srcId, sendData->sspData);

            // Remove message from the list. Max retries were exceeded.
            ListErase(portId, sendData);
            FreeSendData(sendData);
        }
    }
} 

/// Process incoming socket data. Registered listener is notified. Handle 
/// message timeouts and ACK/NAK.
/// @param[in] portId A port identifier. 
static void ProcessReceive(SspPortId portId)
{
    const SspData* sspData = NULL;
    SendData* sendData = NULL;
    SspErr err;
    UINT16 packetCnt = 0;

    // Is there data in the receive buffer?
    if (SSPHAL_IsRecvQueueEmpty(portId) == FALSE)
    {
        // Try to receive a single SSP packet
        err = SSPCOM_ProcessReceive(portId, &sspData, SSP_RECV_TIMEOUT);

        // Receive succeeded?
        if (err == SSP_SUCCESS && sspData)
        {
            // Received message. Decode the message and handle.

            // Did ACK message arrive?
            if (sspData->packet.header.type == MSG_TYPE_ACK)
            {
                // Success! Client's message successfully transmitted over SSP.
                SSP_TRACE_FORMAT("ACK received. Port: %d Socket: %d Trans: %d", 
                    portId, sspData->packet.header.srcId, sspData->packet.header.transId);

                // Find the SendData instance associated with this ACK
                sendData = ListFind(portId, &sspData->packet.header);

                // Free the SendData as the transmission was successful
                if (NULL != sendData)
                {
                    // Let client know remote CPU accepted the message
                    sendData->sspData->err = SSP_SUCCESS;
                    NotifyListener(sendData->sspData->packet.header.srcId, sendData->sspData);

                    // Free allocated memory
                    ListErase(portId, sendData);
                    FreeSendData(sendData);
                }
            }

            // Did NAK message arrive?
            else if (sspData->packet.header.type == MSG_TYPE_NAK)
            {
                SSP_TRACE_FORMAT("NAK received. Port: %d Socket: %d", portId, sspData->packet.header.destId);

                // Find the SendData associated with this NAK
                sendData = ListFind(portId, &sspData->packet.header);

                // Set message state to SEND_STATE to force a retransmission
                if (NULL != sendData)
                    sendData->state = SEND_STATE;
            }

            // Did data message arrive?
            else if (sspData->packet.header.type == MSG_TYPE_DATA)
            {
                SSP_TRACE_FORMAT("Data received. Port: %d Socket: %d Trans: %d", portId, 
                    sspData->packet.header.destId, sspData->packet.header.transId);

                // Is a listener registered on the destination socket?
                if (GetCallbackListener(sspData->packet.header.destId))
                {
                    // ACK the received message
                    SendAck(&sspData->packet.header);

                    // Notify the client that message data received
                    NotifyListener(sspData->packet.header.destId, sspData);
                }
                else
                {
                    // NAK received message. No client to handle message.
                    SendNak(&sspData->packet.header);
                }
            }

            else
            {
                // Unknown message type received. Should never happen.
                SSP_TRACE("Unknown packet received.");
            }
        }
        else if (sspData)
        {
            // For a corrupted message data with header intact, send a NAK to force 
            // sender to retransmit
            if ((SSP_CORRUPTED_PACKET == err || SSP_PARTIAL_PACKET_HEADER_VALID == err) &&
                 MSG_TYPE_DATA == sspData->packet.header.type)
            {
                // Data message received but it was corrupted, send NAK and try again
                SendNak(&sspData->packet.header);
            }

            SSP_TRACE_FORMAT("*** Corrupt data received. Port %d Err %d ***", portId, err);
        }
    }

    // Get the list head
    sendData = ListFront(portId);

    SSPOSAL_LockGet(self.hSspLock, SSP_OSAL_WAIT_DEFAULT);

    // Check for timeouts on all packets waiting for an ACK
    while (NULL != sendData && packetCnt++ < MAX_SEND_DATA_BLOCKS)
    {
        // Packet receive ACK timeout expired?
        if (sendData->state == RECEIVE_STATE &&
            SSPOSAL_GetTickCount() - sendData->sendTickStamp > SSP_ACK_TIMEOUT)
        {
            // Try sending the message again
            sendData->state = SEND_STATE;

            SSP_TRACE_FORMAT("Message timeout. Resend data. Trans: %d Size: %d", 
                sendData->sspData->packet.header.transId,
                sendData->sspData->packetSize);
        }

        // Get the next list structure
        sendData = sendData->next;
    }

    SSPOSAL_LockPut(self.hSspLock);
} 

/// Initialize the port. 
/// @param[in] portId A port identifier.
/// @return SSP_SUCCESS if success. 
SspErr SSP_Init(SspPortId portId)
{
    SspErr err;

    if (self.initOnce == FALSE)
    {
        self.initOnce = TRUE;

        // Initialize allocator module
#ifdef USE_FB_ALLOCATOR
        ALLOC_Init();
#endif
        self.hSspLock = SSPOSAL_LockCreate();

        // Create SspData object for ACK/NAK usage
        self.sspDataForAckNak = (SspData*)self.sspDataForAckNakMem;
        SSPCOM_InitSspData(self.sspDataForAckNak, 0);
    }
    
    err = SSPCOM_Init(portId);
    return err;
}

/// Terminate SSP and cleanup resources. 
void SSP_Term(void)
{
    SendData* sendData;
    UINT16 portId;

    // Iterate over all ports
    for (portId=SSP_PORT1; portId<SSP_MAX_PORTS; portId++)
    {
        // Remove all outgoing messages from list
        while ((sendData = ListFront(portId)) != NULL)
        {
            ListErase((SspPortId)portId, sendData);
            FreeSendData(sendData);
        }
    }

    SSPOSAL_LockDestroy(self.hSspLock);
    self.hSspLock = SSP_OSAL_INVALID_HANDLE_VALUE;
    SSPCOM_Term();
}

/// Open a socket on a port. Each socket may only be opened one time. Socket ID's
/// are not shared across ports. Each socket is unique on a CPU.
/// @param[in] portId A port identifier.
/// @param[in] socketId A socket identifier to open on the port.
/// @return SSP_SUCCESS if success. 
SspErr SSP_OpenSocket(SspPortId portId, UINT8 socketId)
{
    SspErr err;

    // Open the SSP socket
    err = SSPCOM_OpenSocket(portId, socketId);
    return err;
}

/// Close a socket. 
/// @param[in] socketId A socket identifier to close.
/// @return SSP_SUCCESS if success. 
SspErr SSP_CloseSocket(UINT8 socketId)
{
    return SSPCOM_CloseSocket(socketId);
} 

/// Asynchronously send multiple data buffers over a socket. 
/// @param[in] srcSocketId A source socket identifier.
/// @param[in] destSocketId A destination socket identifier.
/// @param[in] numData The number of array elements.
/// @param[in] dataArray An array of data with numData elements. 
/// @param[in] dataSizeArray An array of dataArray sizes with numData elements.
/// @return SSP_SUCCESS if success.
SspErr SSP_SendMultiple(UINT8 srcSocketId, UINT8 destSocketId, INT16 numData, 
    void const** dataArray, UINT16* dataSizeArray)
{
    INT16 i;
    UINT8* dest;
    SspPortId portId;
    SendData* sendData = NULL;
    SspErr err = SSP_SUCCESS;
    UINT16 bytesCopied = 0;
    UINT16 bufSize = 0;
    UINT16 dataSize = 0;

    if (NULL == dataArray || NULL == *dataArray || NULL == dataSizeArray)
        return SSPCMN_ReportErr(SSP_BAD_ARGUMENT);

    // Calculate the total data size
    for (i=0; i<numData; ++i)
        dataSize += dataSizeArray[i];

    // Data size too large?
    if (dataSize > SSP_MAX_BODY_SIZE)
        return SSPCMN_ReportErr(SSP_DATA_SIZE_TOO_LARGE);

    // Get port ID for socket
    err = SSPCOM_GetPortId(srcSocketId, &portId);
    if (err != SSP_SUCCESS)
        return SSPCMN_ReportErr(SSP_BAD_SOCKET_ID);

    // Too many messages waiting?
    if (ListSize(portId) >= SSP_MAX_MESSAGES)
        return SSPCMN_ReportErr(SSP_QUEUE_FULL);

    // Create outgoing send message structure
    sendData = AllocSendData(dataSize);
    if (!sendData)
        return SSPCMN_ReportErr(SSP_OUT_OF_MEMORY);

    // Is there client data?
    if (dataSize > 0)
    {
        // Copy client data into packet body
        dest = sendData->sspData->packet.body;
        for ( i = 0; i < numData; ++i )
        {
            // Compute how much destination buffer remains
            bufSize = dataSize - bytesCopied;

            // Ensure computed value is within dataSize range
            if (bufSize <= dataSize)
            {
                // Copy data into destination packet body buffer
                memcpy(dest, dataArray[i], dataSizeArray[i]);

                dest += dataSizeArray[i];
                bytesCopied += dataSizeArray[i];
            }
            else
            {
                err = SSPCMN_ReportErr(SSP_SOFTWARE_FAULT);
                ASSERT();
                break;
            }
        }
    }

    if (err == SSP_SUCCESS)
    {
        // Fill in packet header information
        sendData->sspData->type = SSP_SEND;
        sendData->sspData->packet.header.bodySize = (UINT8)dataSize;
        sendData->sspData->packet.header.srcId = srcSocketId;
        sendData->sspData->packet.header.destId = destSocketId;
        sendData->sspData->packet.header.transId = self.sendTransId++;
        sendData->sspData->packet.header.type = MSG_TYPE_DATA;

        // Insert the outgoing message into the list
        ListInsert(portId, sendData);

        // Disable power savings for outgoing message
        SSPHAL_PowerSave(FALSE);
    }
    else 
    {
        // Free allocated memory if not successful  
        FreeSendData(sendData);       
    }

    return err;
} 

/// Asynchronously send data over a socket. The registered callback on SSP_Listener()
/// will be invoked upon success or failure of the sent message. 
/// @param[in] srcSocketId A source socket identifier.
/// @param[in] destSocketId A destination socket identifier.
/// @param[in] data The data to send. 
/// @param[in] dataSize The size of data in bytes.
/// @return SSP_SUCCESS if success.
SspErr SSP_Send(UINT8 srcSocketId, UINT8 destSocketId, const void* data, UINT16 dataSize)
{
    return SSP_SendMultiple(srcSocketId, destSocketId, 1, &data, &dataSize);
}

/// Register to listen for incoming data on a socket. Called when either: a
/// valid incoming packet arrives, an outgoing data packet is acknowledged by 
/// the remote, or an outgoing packet send fails. 
/// @param[in] socketId A socket identifier.
/// @param[in] callback The callback function pointer.
/// @param[in] userData Optional user data passed back to the callback function.
/// @return SSP_SUCCESS if success.
SspErr SSP_Listen(UINT8 socketId, SspDataCallback callback, void* userData)
{
    SspErr err = SSP_SUCCESS;

    if (!self.initOnce)
        return SSPCMN_ReportErr(SSP_NOT_INITIALIZED);

    if (!callback)
        return SSPCMN_ReportErr(SSP_BAD_ARGUMENT);

    if (SSPCOM_IsSocketOpen(socketId) == FALSE)
        return SSPCMN_ReportErr(SSP_SOCKET_NOT_OPEN);

    SSPOSAL_LockGet(self.hSspLock, SSP_OSAL_WAIT_DEFAULT);

    // Is the callback map entry empty?
    if (!self.socketToCallbackMap[socketId])
    {
        // Save the caller's callback function pointer and user data
        self.socketToCallbackMap[socketId] = callback;
        self.socketToUserDataMap[socketId] = userData;
    }
    else
    {
        // Overwriting an existing callback listener is not allowed.
        err = SSPCMN_ReportErr(SSP_DUPLICATE_LISTENER);
    }

    SSPOSAL_LockPut(self.hSspLock);
    return err;
}

/// Get the number of messages in the send queue. 
/// @param[in] portId A port identifier.
/// @return The number of messages in the queue.
UINT16 SSP_GetSendQueueSize(SspPortId portId)
{
    UINT16 size = ListSize(portId);
    return size;
}

/// Get the receive queue empty status. 
/// @param[in] portId A port identifier.
/// @return TRUE if incoming receive queue is empty. 
BOOL SSP_IsRecvQueueEmpty(SspPortId portId)
{
    return SSPHAL_IsRecvQueueEmpty(portId);
}

/// Called periodically from a single task or loop to process SSP packets.
/// Clients registered with SSP_Listener() are called back from the context
/// that calls this function. This function only needs to be called if there
/// are outgoing or incoming data to be processed.
void SSP_Process()
{
    UINT16 portId = 0;
    BOOL powerSave = TRUE;

    // Iterate over all ports
    for (portId=SSP_PORT1; portId<SSP_MAX_PORTS; portId++)
    {
        // Is the port open?
        if (SSPCOM_IsPortOpen((SspPortId)portId) == TRUE)
        {
            // Process incoming data on the specified port
            ProcessReceive((SspPortId)portId);

            // Process outgoing data on the specified port
            ProcessSend((SspPortId)portId);

            // Messages still in outgoing list?
            if (ListSize((SspPortId)portId) > 0)
            {
                // No power savings - still outgoing messages to process
                powerSave = FALSE;
            }
        }
    }

    // Can power savings be enabled?
    if (powerSave)
    {
        // Enable SSP power savings. No more outgoing messages.
        SSPHAL_PowerSave(TRUE);
    }
}

/// Set an error handler callback function.
/// @param[in] handler The handler callback function. 
void SSP_SetErrorHandler(ErrorHandler handler)
{
    SSPCMN_SetErrorHandler(handler);
}

/// Get the last SSP error code.
/// @return The last error generated within SSP.
SspErr SSP_GetLastErr(void)
{
    return SSPCMN_GetLastErr();
}

