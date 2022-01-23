#ifndef _CRC_H
#define _CRC_H

#ifdef __cplusplus
extern "C" {
#endif

unsigned short Crc16CalcBlock(unsigned char* crc_msg, int len,
    unsigned short crc);

#ifdef __cplusplus 
}
#endif

#endif 
