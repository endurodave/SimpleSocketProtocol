#include "ssp_com.h"
#include "ssp_common_p.h"
#include "ssp_hal.h"
#include "ssp_osal.h"
#include "ssp_util.h"
#include "ssp_crc.h"
#include "ssp_fault.h"
#ifdef USE_FB_ALLOCATOR
#include "fb_allocator.h"
#else
#include <stdlib.h>
#endif

typedef enum
{
    PS_SIGNATURE_1,
    PS_SIGNATURE_2,
    PS_DESTINATION,
    PS_SOURCE,
    PS_TYPE,
    PS_BODY_SIZE,
    PS_TRANSACTION,
    PS_CHECKSUM,
    PS_BODY,
    PS_FOOTER_1,
    PS_FOOTER_2
} ParseState;

typedef UINT16 SspPacketFooterType;

// Maximum number of SspData fixed blocks
#define MAX_SSP_DATA_BLOCKS     SSP_MAX_MESSAGES

// Maximum number of bytes to read from communication port on each
// call to SSPHAL_PortRecv()
#define MAX_PORT_RECV_BYTES     1

// If communication port driver guarantees one full SSP message when
// SSPHAL_PortRecv() called (like maybe a DMA SPI driver)
//#define MAX_PORT_RECV_BYTES     SSP_PACKET_SIZE(SSP_MAX_BODY_SIZE)

// Define the fixed block allocator and memory for dynamic SspData. Add +1 to number 
// of blocks to account for one dedicated block for receiving data (i.e. self.sspDataRecv).
#ifdef USE_FB_ALLOCATOR
ALLOC_DEFINE(sspDataAllocator, SSP_DATA_SIZE(SSP_MAX_BODY_SIZE), MAX_SSP_DATA_BLOCKS + 1)
#endif

// First 2 packet header synchronization bytes
#define SIG_1   0xBE
#define SIG_2   0xEF

typedef struct
{
    // Socket ID to port ID mapping
    SspPortId socketToPortIdMap[SSP_SOCKET_MAX];

    // Software lock
    SSP_OSAL_HANDLE hSspLock;

    // Parse data
    ParseState parseState;
    SspPacketFooterType currentFooter;
    SspData* sspDataRecv;
    UINT16 parseBytes;

    // Set TRUE after one time initialization complete
    BOOL initOnce;
} SspComObj;

// Private module data
static SspComObj self;

// Private functions
static UINT8 Checksum(const UINT8* data, UINT16 dataSize);
static void ParseReset(void);
static BOOL Parse(const UINT8* buf, UINT16 bufSize, UINT16* bytesParsed);
static SspErr Receive(SspPortId portId, const SspData** sspData, UINT16 timeout);

/// Compute 8-bit checksum.
/// @param[in] data Data bytes to compute checkum over.
/// @param[in] dataSize Number of bytes pointed to by data argument.
/// @return The checksum.
static UINT8 Checksum(const UINT8* data, UINT16 dataSize)
{
    UINT8 sum = 0;
    for (UINT16 i = 0; i < dataSize; i++)
        sum += data[i];
    return sum;
}

/// Receive data on a port. 
/// @param[in] portId A port identifier.
/// @param[out] sspData The received data. 
/// @param[in] timeout The timeout to receive data in mS.
static SspErr Receive(SspPortId portId, const SspData** sspData, UINT16 timeout)
{
    const char* parseData = NULL;
    static char dataRecv[MAX_PORT_RECV_BYTES];
    UINT16 bytesRead = 0;
    UINT16 bytesParsed = 0;
    BOOL complete = TRUE; 
    BOOL readFromPort = TRUE;
    static const UINT16 PARSE_HISTORY_SIZE = sizeof(SspPacketHeader);
    static char parseHistory[sizeof(SspPacketHeader)];
    static UINT16 parseHistoryIdx = 0;

    if (NULL == sspData)
        return SSP_BAD_ARGUMENT;

    do
    {
        if (readFromPort)
        {
            // Read data from port
            SSPHAL_PortRecv(portId, dataRecv, &bytesRead, MAX_PORT_RECV_BYTES, timeout);
            parseData = dataRecv;
        }
        else
        {
            // Read data from parse history skipping first byte. Trying to relocate the sync bytes.
            parseData = &parseHistory[1];
            bytesRead = PARSE_HISTORY_SIZE - 1;

            // Reset history variables
            readFromPort = TRUE;
            parseHistoryIdx = 0;
        }

        // Is there data to parse?
        if (bytesRead > 0)
        {
            // Parse the packet data
            complete = Parse((UINT8*)parseData, bytesRead, &bytesParsed);

            // Save parse data history incase need to reparse header
            for (int i = 0; i < bytesRead; i++)
            {
                if (parseHistoryIdx >= PARSE_HISTORY_SIZE)
                    break;
                parseHistory[parseHistoryIdx++] = parseData[i];
            }

            // If checksum header is bad, need to backup and reparse the header data
            // history and try to find sync bytes
            if (complete && self.sspDataRecv->err == SSP_BAD_HEADER_CHECKSUM &&
                parseHistoryIdx == PARSE_HISTORY_SIZE)
            {
                complete = FALSE;
                readFromPort = FALSE;
            }
        }
        else
        {
            // No more data to parse
            break;
        }
    } while (!complete);

    // Return a pointer to the internal receive structure
    *sspData = self.sspDataRecv;

    return self.sspDataRecv->err;
}

/// Reset the parser state machine.
static void ParseReset(void)
{
    self.parseState = PS_SIGNATURE_1;
    self.parseBytes = 0;
} 

/// Packet parser state machine. 
/// @param[in] buf Data to parse.
/// @param[in] bufSize Size of data to parse. 
/// @param[out] bytesParsed The number of bytes parsed.
/// @return TRUE if parsing complete either by success or failure.
static BOOL Parse(const UINT8* buf, UINT16 bufSize, UINT16* bytesParsed)
{
    BOOL parseComplete = FALSE;
    const UINT8* p;
    SspPacketFooterType crc = 0;
    UINT8* body = NULL;

    if (buf == NULL || bytesParsed == NULL || bufSize == 0)
        return TRUE;

    // Iterate over all bytes in the buffer or until parseComplete is TRUE
    for (p = buf, *bytesParsed = 0; !parseComplete && *bytesParsed<bufSize; p++, (*bytesParsed)++)
    {
        // SSP packet parse state machine
        switch (self.parseState)
        {
        case PS_SIGNATURE_1:
            self.sspDataRecv->err = SSP_PARTIAL_PACKET;
            self.sspDataRecv->packet.header.sig[0] = SIG_1;
            self.sspDataRecv->packet.header.sig[1] = SIG_2;
            if (*p == SIG_1)
            {
                self.parseState = PS_SIGNATURE_2;
            }
            else
            {
                self.sspDataRecv->err = SSP_BAD_SIGNATURE;
                ParseReset();
            }
            break;
        case PS_SIGNATURE_2:
            if (*p == SIG_2)
            {
                self.parseState = PS_DESTINATION;
            }
            else if (*p == SIG_1)
            {
                self.parseState = PS_SIGNATURE_2;
            }
            else
            {
                self.sspDataRecv->err = SSP_BAD_SIGNATURE;
                ParseReset();
            }
            break;
        case PS_DESTINATION:
            self.sspDataRecv->packet.header.destId = *p;
            self.parseState = PS_SOURCE;
            break;
        case PS_SOURCE:
            self.sspDataRecv->packet.header.srcId = *p;
            self.parseState = PS_TYPE;
            break;
        case PS_TYPE:
            self.sspDataRecv->packet.header.type = *p;
            self.parseState = PS_BODY_SIZE;
            break;
        case PS_BODY_SIZE:
            self.sspDataRecv->packet.header.bodySize = *p;
            self.parseState = PS_TRANSACTION;
            break;
        case PS_TRANSACTION:
            self.sspDataRecv->packet.header.transId = *p;
            self.parseState = PS_CHECKSUM;
            break;
        case PS_CHECKSUM:
            // Is header checksum valid?
            self.sspDataRecv->packet.header.checksum = *p;
            if (*p == Checksum((UINT8*)&self.sspDataRecv->packet.header, sizeof(SspPacketHeader)-sizeof(UINT8)))
            {
                // Valid header checksum
                self.sspDataRecv->err = SSP_PARTIAL_PACKET_HEADER_VALID;

                // Is incoming body data within the max allowable size?
                if (self.sspDataRecv->packet.header.bodySize <= SSP_MAX_BODY_SIZE)
                {
                    self.parseState = PS_BODY;
                }
                else
                {
                    // Body too large
                    self.sspDataRecv->err = SSP_PACKET_TOO_LARGE;
                    ParseReset();
                    parseComplete = TRUE;
                }
            }
            else
            {
                // Invalid header checksum
                self.sspDataRecv->err = SSP_BAD_HEADER_CHECKSUM;
                ParseReset();
                parseComplete = TRUE;
            }
            break;
        case PS_BODY:
            if (self.sspDataRecv->packet.header.bodySize)
            {
                // Get body pointer
                body = (UINT8*)(self.sspDataRecv->packet.body);
                if (body)
                {
                    body[self.parseBytes] = *p;
                    if (++self.parseBytes >= self.sspDataRecv->packet.header.bodySize)
                    {
                        self.parseState = PS_FOOTER_1;
                    }
                }
                else
                {
                    // Body pointer was null but header bodySize was not 0.
                    // This should never happen.
                    self.sspDataRecv->err = SSP_PARSE_ERROR;
                    ParseReset();
                    parseComplete = TRUE;
                }
                break;
            }
            else
            {
                self.parseState = PS_FOOTER_1;
            }
            // Fall through.
        case PS_FOOTER_1:
            self.currentFooter = *p;
            self.parseState = PS_FOOTER_2;
            break;
        case PS_FOOTER_2:
        {
            // Is the socket ID out of range?
            if (self.sspDataRecv->packet.header.destId >= SSP_SOCKET_MAX)
            {
                // Socket is not valid error
                self.sspDataRecv->err = SSP_BAD_SOCKET_ID;
            }
            else if (self.socketToPortIdMap[self.sspDataRecv->packet.header.destId] == SSP_INVALID_PORT)
            {
                // Socket is not open error
                self.sspDataRecv->err = SSP_SOCKET_NOT_OPEN;
            }
            else
            {
                // Packet received.

                // Compute CRC on incoming packet header
                self.currentFooter += (UINT16)*p << 8;
                crc = Crc16CalcBlock((unsigned char*)&self.sspDataRecv->packet.header,
                    sizeof(SspPacketHeader) + self.sspDataRecv->packet.header.bodySize, 0xFFFF);

                // If not little-endian
                if (!LE())
                {
                    // Convert CRC to little-endian
                    crc = bswap16(crc);
                }

                // Does computed and received CRC match?
                if (self.currentFooter == crc)
                {
                    // Packet received successfully
                    self.sspDataRecv->err = SSP_SUCCESS;
                    *self.sspDataRecv->crc = crc;
                }
                else
                {
                    // Corrupted packet
                    self.sspDataRecv->err = SSP_CORRUPTED_PACKET;
                }
            }
            ParseReset();
            parseComplete = TRUE;
            break;
        }
        default:
            ASSERT();
            ParseReset();
        }
    }

    return parseComplete;
}

/// Initialize and open the port.
/// @param[in] portId A port identifier. 
/// @return SSP_SUCCESS if success. 
SspErr SSPCOM_Init(SspPortId portId)
{
    SspErr err = SSP_SUCCESS;
    BOOL success;

    SSPHAL_Init(portId);

    if (self.initOnce == FALSE)
    {
        self.initOnce = TRUE;

        SSPOSAL_Init();

        self.parseState = PS_SIGNATURE_1;
        self.hSspLock = SSPOSAL_LockCreate();

        // Allocate a SspData structure
        self.sspDataRecv = SSPCOM_AllocateSspData(SSP_MAX_BODY_SIZE);
        ASSERT_TRUE(self.sspDataRecv);
        if (self.sspDataRecv)
        {
            self.sspDataRecv->type = SSP_RECEIVE;
        }
    }

    // Open the SSP port
    success = SSPHAL_PortOpen(portId);
    if (success == FALSE)
        err = SSP_PORT_OPEN_FAILED;

    return err;
}

/// Terminate and cleanup resources. 
void SSPCOM_Term(void)
{
    if (self.sspDataRecv != NULL)
    {
        SSPCOM_DeallocateSspData(self.sspDataRecv);
        self.sspDataRecv = NULL;
    }

    SSPOSAL_LockDestroy(self.hSspLock);
    self.hSspLock = SSP_OSAL_INVALID_HANDLE_VALUE;

    SSPHAL_Term();
    SSPOSAL_Term();
}

/// Allocate packet body data.
/// @param[in] dataSize Size of the data body payload (i.e. client data).
SspData* SSPCOM_AllocateSspData(UINT16 dataSize)
{
    SspData* sspData = NULL;

    // Allocate memory space
#ifdef USE_FB_ALLOCATOR
    sspData = (SspData*)ALLOC_Calloc(sspDataAllocator, 1, SSP_DATA_SIZE(dataSize));
#else
    sspData = (SspData*)calloc(1, SSP_DATA_SIZE(dataSize));
#endif
    if (sspData != NULL)
    {
        SSPCOM_InitSspData(sspData, dataSize);
    }

    return sspData;
}

/// Deallocate previously allocated data with SSPCOM_AllocateSspData().
/// @param[in] sspData The data to deallocate. 
void SSPCOM_DeallocateSspData(SspData* sspData)
{
#ifdef USE_FB_ALLOCATOR
    ALLOC_Free(sspDataAllocator, sspData);
#else
    free(sspData);
#endif
}

/// Initialize SspData structure.
/// @param[out] sspData The data to initialize.
/// @param[in] dataSize The packet body size.
SspData* SSPCOM_InitSspData(SspData* sspData, UINT16 dataSize)
{
    ASSERT_TRUE(sspData);

    // Set packet size
    sspData->packetSize = SSP_PACKET_SIZE(dataSize);

    // Point the CRC at the end of client data
    sspData->crc = (UINT16*)&sspData->packet.body[dataSize];

    return sspData;
}

/// Close the socket.
/// @param[in] socketId A socket identifier.
/// @return SSP_SUCCESS if success. 
SspErr SSPCOM_CloseSocket(UINT8 socketId)
{
    if (socketId >= SSP_SOCKET_MAX)
        return SSP_BAD_SOCKET_ID;

    SSPOSAL_LockGet(self.hSspLock, SSP_OSAL_WAIT_DEFAULT);

    // Remove socket from map. The socket is free.
    self.socketToPortIdMap[socketId] = SSP_INVALID_PORT;

    SSPOSAL_LockPut(self.hSspLock);
    return SSP_SUCCESS;
}

/// Open a socket.
/// @param[in] A port identifier. 
/// @param[in] A socket identifier. 
SspErr SSPCOM_OpenSocket(SspPortId portId, UINT8 socketId)
{
    if (!SSPCOM_IsPortOpen(portId))
        return SSP_PORT_NOT_OPEN;

    if (socketId >= SSP_SOCKET_MAX)
        return SSP_BAD_SOCKET_ID;

    if (SSPCOM_IsSocketOpen(socketId))
        return SSP_SOCKET_ALREADY_OPEN;

    SSPOSAL_LockGet(self.hSspLock, SSP_OSAL_WAIT_DEFAULT);

    // A socket to a port ID. The socket is in use.
    self.socketToPortIdMap[socketId] = portId;

    SSPOSAL_LockPut(self.hSspLock);
    return SSP_SUCCESS;
}

/// Get the port open state.
/// @param[in] A port identifier. 
/// @return TRUE if port is open.
BOOL SSPCOM_IsPortOpen(SspPortId portId)
{
    return SSPHAL_PortIsOpen(portId);
}

/// Get the socket open state.
/// @param[in] socketId A socket identifier.
/// @return TRUE if the socket is open.
BOOL SSPCOM_IsSocketOpen(UINT8 socketId)
{
    BOOL isOpen;

    if (socketId >= SSP_SOCKET_MAX)
        return FALSE;

    SSPOSAL_LockGet(self.hSspLock, SSP_OSAL_WAIT_DEFAULT);

    if (self.socketToPortIdMap[socketId] == SSP_INVALID_PORT)
        isOpen = FALSE;
    else
        isOpen = TRUE;

    SSPOSAL_LockPut(self.hSspLock);
    return isOpen;
}

/// Get the port identifer assigned to a socket.
/// @param[in] socketId A socket identifier. 
/// @param[out] portId The port assigned to the socket.
/// @return SSP_SUCCESS if success.
SspErr SSPCOM_GetPortId(UINT8 socketId, SspPortId* portId)
{
    SspErr err;

    if (portId == NULL)
        return SSP_BAD_ARGUMENT;

    if (socketId >= SSP_SOCKET_MAX)
        return SSP_BAD_SOCKET_ID;

    SSPOSAL_LockGet(self.hSspLock, SSP_OSAL_WAIT_DEFAULT);

    if (self.socketToPortIdMap[socketId] == SSP_INVALID_PORT)
    {
        err = SSP_SOCKET_NOT_OPEN;
    }
    else
    {
        *portId = self.socketToPortIdMap[socketId];
        err = SSP_SUCCESS;
    }

    SSPOSAL_LockPut(self.hSspLock);
    return err;
}

/// Flush data on a port.
/// @param[in] portId A port identifier.
/// @erturn SSP_SUCCESS if success. 
SspErr SSPCOM_Flush(SspPortId portId)
{
    SSPHAL_PortFlush(portId);
    return SSP_SUCCESS;
}

/// Send data over a socket.
/// @param[in] sspData The data to send.
/// @return SSP_SUCCESS if success.
SspErr SSPCOM_Send(SspData* sspData)
{
    SspPortId portId;
    SspErr err;
    BOOL success;

    if (NULL == sspData)
        return SSP_BAD_ARGUMENT;

    if (sspData->packet.header.srcId >= SSP_SOCKET_MAX)
        return SSP_BAD_SOCKET_ID;

    if (!SSPCOM_IsSocketOpen(sspData->packet.header.srcId))
        return SSP_SOCKET_NOT_OPEN;

    // Get the port assigned to this socket
    err = SSPCOM_GetPortId(sspData->packet.header.srcId, &portId);
    if (SSP_SUCCESS != err)
        return SSP_BAD_SOCKET_ID;

    // Is port open?
    if (!SSPCOM_IsPortOpen(portId))
        return SSP_PORT_NOT_OPEN;

    // Fill in the rest of the packet header
    sspData->packet.header.sig[0] = SIG_1;
    sspData->packet.header.sig[1] = SIG_2;
    sspData->packet.header.checksum =
        Checksum((UINT8*)&sspData->packet.header, sizeof(SspPacketHeader)-sizeof(UINT8));

    // Compute the CRC for outgoing packet
    *sspData->crc = Crc16CalcBlock((UINT8*)&sspData->packet.header,
        sizeof(sspData->packet.header) + sspData->packet.header.bodySize, 0xFFFF);

    // If not little-endian
    if (!LE())
    {
        // Convert CRC to little-endian
        *sspData->crc = bswap16(*sspData->crc);
    }

    // Send the entire packet including header, body and CRC
    success = SSPHAL_PortSend(portId, (const char*)&sspData->packet, sspData->packetSize);
    if (success)
        err = SSP_SUCCESS;
    else
        err = SSP_SEND_FAILURE;

    return err;
}

/// Process incoming receive data. 
/// @param[in] portId A port identifier.
/// @param[out] sspData Incoming data. 
/// @param[in] timeout Timeout in mS.
/// @return SSP_SUCCESS if success.
SspErr SSPCOM_ProcessReceive(SspPortId portId, const SspData** sspData, UINT16 timeout)
{
    SspErr err;

    if (!SSPCOM_IsPortOpen(portId))
        return SSP_PORT_NOT_OPEN;

    // Receive one packet on the specified port
    err = Receive(portId, sspData, timeout);
    return err;
}

