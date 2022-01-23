#include "ssp_common.h"
#include "ssp_osal.h"
#include <stdio.h>
#include <stdarg.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

// Trace output used for debugging
void SSP_TraceFormat(const char* format, ...)
{
    SSPOSAL_EnterCritical();

    va_list vl;
    va_start(vl, format);

#ifdef ARDUINO
    char buf[128];
    vsprintf(buf, format, vl);
    Serial.println(buf);
#else
    vprintf(format, vl);
#endif

    va_end(vl);

#ifndef ARDUINO
    printf("\n");
#endif

    SSPOSAL_ExitCritical();
}

// Trace output used for debugging
void SSP_Trace(const char* str)
{
    SSPOSAL_EnterCritical();

#ifdef ARDUINO
    Serial.println(str);
#else
    printf(str);
    printf("\n");
#endif

    SSPOSAL_ExitCritical();
}
