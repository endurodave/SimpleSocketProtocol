#include "ssp_common_p.h"

static ErrorHandler errorHandlerFunc = 0;
static SspErr lastErr = SSP_SUCCESS;

// Report an SSP error
SspErr SSPCMN_ReportErr(SspErr err)
{
    lastErr = err;
    ErrorHandler handler = errorHandlerFunc;

    // If a callback handler defined notify registered function
    if (handler)
        handler(err);

    return err;
}

// Get the last SSP error
SspErr SSPCMN_GetLastErr(void)
{    
    return lastErr;
}

void SSPCMN_SetErrorHandler(ErrorHandler handler)
{
    errorHandlerFunc = handler;
}