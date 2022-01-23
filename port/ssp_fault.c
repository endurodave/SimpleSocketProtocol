// Fault handler implemention of ssp_fault.h interface

#include "ssp_fault.h"
#include "ssp_common.h"
#include <assert.h>
#if WIN32
	#include "windows.h"
#endif

//----------------------------------------------------------------------------
// FaultHandler
//----------------------------------------------------------------------------
void FaultHandler(const char* file, unsigned short line)
{
#if WIN32
	// If you hit this line, it means one of the ASSERT macros failed.
    DebugBreak();
#endif

    SSP_TRACE_FORMAT("Fault: %s line %d\n", file, line);

	assert(0);
}