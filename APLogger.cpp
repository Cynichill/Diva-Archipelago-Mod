#include "APLogger.h"
#include <windows.h>
#include <iostream>

void APLogger::print(const char* const fmt, ...)
{
    static bool console = GetConsoleWindow();

    if (!console)
        return;

    printf("[Archipelago] ");
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
