#include "ssp_util.h"
#include "ssp_fault.h"
#include <string.h>

/// Determine big/little endian
/// @return Return true if processor is little-endian.
bool LE()
{
    const int n = 1;
    const bool le = (*(char *)&n == 1);
    return le;
}

/// Swap bytes between big/little endian
/// @num The number to byte swap.
/// @return The byte swapped value.
unsigned short bswap16(unsigned short num)
{
    unsigned short res = 0;
    unsigned short b0 = (num & 0xff) << 8; 
    unsigned short b1 = (num & 0xff00) >> 8;
    res = b0 | b1;
    return res;
}
