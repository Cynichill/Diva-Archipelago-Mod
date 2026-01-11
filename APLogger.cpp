#include "APLogger.h"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <stdarg.h>

void APLogger::print(const char* const fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    if (GetConsoleWindow() != NULL) {
        printf("[Archipelago] ");
        vprintf(fmt, args);
    }

    va_end(args);

    // Log to file
    /*FILE* log = fopen("AP.txt", "a");

    if (log != NULL) {
        fprintf(log, "[Archipelago] ");
        vfprintf(log, fmt, args);
        fclose(log);
    }*/



    // Something is causing problems with the output reaching the console under Wine.
    // Probably the PreInit in dllmainDebug. "Temporary" long term fix.
    fflush(stdout);
}
