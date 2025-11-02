#include "APLogger.h"
#include <windows.h>
#include <iostream>

void APLogger::print(const char* const fmt, ...)
{
    if (GetConsoleWindow() != NULL) {
        printf("[Archipelago] ");
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }

    // Something is causing problems with the output reaching the console under Wine.
    // Probably the PreInit in dllmainDebug. "Temporary" long term fix.
    fflush(stdout);
}
