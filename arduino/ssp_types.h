#ifndef _DATA_TYPES_H
#define _DATA_TYPES_H

#if WIN32
	#include "windows.h"
#else
	#include <stdint.h>
	typedef signed char INT8;
	typedef unsigned char UINT8;
	typedef signed short INT16;
	typedef unsigned short UINT16;
	// Fixed width: int is 16 bits on AVR (Arduino Uno/Mega), so "unsigned int"
	// would make these 16-bit there
	typedef uint32_t UINT32;
	typedef int32_t INT32;
	typedef char CHAR;
	typedef short SHORT;
	typedef long LONG;
	typedef int INT;
	typedef unsigned int UINT;
	typedef unsigned long DWORD;
	typedef unsigned char BYTE;
	typedef unsigned short WORD;
	typedef float FLOAT;
	typedef double DOUBLE;
	typedef int BOOL;

	#ifndef NULL
	#ifdef __cplusplus
	#define NULL    0
	#else
	#define NULL    ((void *)0)
	#endif
	#endif

	#ifndef FALSE
	#define FALSE               0
	#endif

	#ifndef TRUE
	#define TRUE                1
	#endif
#endif

#endif
