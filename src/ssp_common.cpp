#include "ssp_common.h"
#include "ssp_osal.h"
#include <stdio.h>
#include <stdarg.h>
#ifdef ARDUINO
#include "ssp_arduino.h"

// Where trace text goes; SSP_ArduinoSetTrace(nullptr) turns it off
static Print* traceOut = &Serial;

void SSP_ArduinoSetTrace(Print* out)
{
    traceOut = out;
}
#endif

// Trace output used for debugging
void SSP_TraceFormat(const char* format, ...)
{
#ifdef ARDUINO
    if (traceOut == nullptr)
        return;
#endif

    SSPOSAL_EnterCritical();

    va_list vl;
    va_start(vl, format);

#ifdef ARDUINO
    char buf[128];
    vsnprintf(buf, sizeof(buf), format, vl);
    traceOut->println(buf);
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
#ifdef ARDUINO
    if (traceOut == nullptr)
        return;
#endif

    SSPOSAL_EnterCritical();

#ifdef ARDUINO
    traceOut->println(str);
#else
    printf("%s", str);
    printf("\n");
#endif

    SSPOSAL_ExitCritical();
}
