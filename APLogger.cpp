#include "APLogger.h"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <stdarg.h>
#include <ctime>

namespace APLogger
{
    void print(const char* const fmt, ...)
    {
        if (GetConsoleWindow() == NULL) {
            return;
        }
        else {
            freopen("CONOUT$", "w", stdout);
        }

        va_list args;
        va_start(args, fmt);

        time_t t = time(NULL);
        struct tm tm;
        localtime_s(&tm, &t);

        char buf[64];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);

        printf("[Archipelago] [%s] ", buf);
        vprintf(fmt, args);
        fflush(stdout);

        /*else {
            FILE* log = fopen("AP.txt", "a");

            if (log != NULL) {
                fprintf(log, "[Archipelago] [%s] ", buf);
                vfprintf(log, fmt, args);
                fclose(log);
            }
        }*/

        va_end(args);
    }
}
