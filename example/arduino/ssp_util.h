#ifndef _SSP_UTIL_H
#define _SSP_UTIL_H

#include "ssp_types.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

// True if little-endian
bool LE();

// Byte-swap to/from little/big endian
unsigned short bswap16(unsigned short num);

#ifdef __cplusplus 
}
#endif

#endif 
