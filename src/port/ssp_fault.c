// Fault handler implemention of ssp_fault.h interface

#include "ssp_fault.h"
#include "ssp_common.h"
#include <assert.h>
#include <stdlib.h>
#if WIN32
	#include "windows.h"
#endif

// The default FaultHandler. On GCC and Clang it is a weak symbol: define your
// own FaultHandler() (e.g. to log and reset the board) and it replaces this
// one. That also lets SSP link with other libraries that provide a
// FaultHandler (e.g. C_Allocator).
#if defined(__GNUC__) || defined(__clang__)
	#define SSP_FAULT_WEAK __attribute__((weak))
#else
	#define SSP_FAULT_WEAK
#endif

//----------------------------------------------------------------------------
// FaultHandler
//----------------------------------------------------------------------------
SSP_FAULT_WEAK void FaultHandler(const char* file, unsigned short line)
{
    (void)file;
    (void)line;

    SSP_TRACE_FORMAT("Fault: %s line %d\n", file, line);

#if WIN32
	// If you hit this line, it means one of the ASSERT macros failed.
    // Without a debugger attached, DebugBreak() would just crash the program.
    if (IsDebuggerPresent())
        DebugBreak();
#endif

	assert(0);

	// assert() does nothing with NDEBUG (e.g. the STM32 Arduino core defines
	// it): a fault must not return to the code that detected it
	abort();
}
