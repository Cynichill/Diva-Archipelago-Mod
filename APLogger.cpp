#include "APLogger.h"
#include <stdarg.h>

namespace APLogger
{
    bool logToFile = false;

    std::ofstream APLog;
    const std::filesystem::path LogPath = std::filesystem::current_path() / "log.txt";

    void config(toml::v3::ex::parse_result& data)
    {
        logToFile = data["logging"].value_or(false);
    }

    void print(const char* const fmt, ...)
    {
        if (!logToFile && !GetConsoleWindow())
            return;

        char line[512];

        time_t t = time(NULL);
        struct tm tm;
        localtime_s(&tm, &t);

        char ts[64];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
        int offset = snprintf(line, sizeof(line), "[Archipelago] [%s] ", ts);

        va_list args;
        va_start(args, fmt);
        vsnprintf(line + offset, sizeof(line) - offset, fmt, args);
        va_end(args);

        if (GetConsoleWindow())
            printf("%s", line);

        if (logToFile) {
            APLog.open(LogPath, std::ofstream::out | std::ofstream::app);

            if (APLog.is_open()) {
                APLog.write(line, strlen(line));
                APLog.close();
            }
        }
    }

    void ImGuiTab()
    {
        if (ImGui::BeginTabItem("Mod Log")) {


            ImGui::EndTabItem();
        }
    }
}
