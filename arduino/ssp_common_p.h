// SSP common private header

#ifndef SSP_COMMON_P_H
#define SSP_COMMON_P_H

#include "ssp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

SspErr SSPCMN_ReportErr(SspErr err);
SspErr SSPCMN_GetLastErr(void);
void SSPCMN_SetErrorHandler(ErrorHandler handler);

// Maximum client data payload size in bytes within an SSP packet
#define SSP_MAX_BODY_SIZE	(SSP_MAX_PACKET_SIZE - sizeof(SspPacketHeader) - sizeof(UINT16))

// Total SSP data size is SspData + body data + CRC
#define SSP_DATA_SIZE(_size_)    (sizeof(SspData) + _size_ + sizeof(UINT16))

// Total SSP packet size is SspPacket + body data + plus CRC
#define SSP_PACKET_SIZE(_size_)  (sizeof(SspPacket) + _size_ + sizeof(UINT16))

// The SSP packet header data structure
typedef struct
{
    UINT8 sig[2];
    UINT8 destId;
    UINT8 srcId;
    UINT8 type;
    UINT8 bodySize;
    UINT8 transId;
    UINT8 checksum;
} SspPacketHeader;

// The variable length binary data packet including header, 
// body (i.e. client data) and CRC. MUST be created using 
// SSPCOM_AllocateSspData().
typedef struct
{
    SspPacketHeader header;

    // The body element is a flexible array member. A variable amount
    // of extra memory is allocated to include the client data plus CRC.
    // Must be the last element of the struct.
    UINT8 body[0];
} SspPacket;

// The SSP data structure holding the actual packet and
// other related data.
typedef struct SspData
{
    SspErr err;

    // Send or receive data type
    SspDataType type;

    // Points to CRC location within SspPacket
    UINT16* crc;

    // Total size of the SspPacket in bytes
    UINT16 packetSize;

    // Variable length packet. Must be last element in this structure.
    SspPacket packet;
} SspData;

#ifdef __cplusplus
}
#endif

#endif // SSP_COMMON_P_H
