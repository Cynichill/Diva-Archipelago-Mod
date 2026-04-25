#include "APClient.h"
#include "APLogger.h"
#include <stdarg.h>

namespace APLogger
{
    bool logToFile = false;
    std::string APLogLocal;

    std::ofstream APLog;
    const std::filesystem::path LogPath = std::filesystem::current_path() / "log.txt";

    void config(toml::v3::ex::parse_result& data)
    {
        logToFile = data["logging"].value_or(false);
    }

    void print(const char* const fmt, ...)
    {
        char line[512];

        time_t t = time(NULL);
        struct tm tm;
        localtime_s(&tm, &t);

        char ts[64];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
        int offset = snprintf(line, sizeof(line), "[%s] ", ts);

        va_list args;
        va_start(args, fmt);
        vsnprintf(line + offset, sizeof(line) - offset, fmt, args);
        va_end(args);

        APLogLocal += std::string(line);

        if (GetConsoleWindow())
            printf("[Archipelago] %s", line);

        if (logToFile) {
            APLog.open(LogPath, std::ofstream::out | std::ofstream::app);

            if (APLog.is_open()) {
                APLog.write(line, strlen(line));
                APLog.close();
            }
        }
    }

    // Previously considered for a tab, made a collapsable header instead
    void ImGuiTab()
    {
        /*if (!APClient::devMode)
            return;*/

        if (!ImGui::CollapsingHeader("Logging")) {
            // TODO: Check write perms?
            ImGui::Checkbox("Log to file", &APLogger::logToFile);
            if (&APLogger::logToFile) {
                ImGui::SameLine();
                ImGui::TextLinkOpenURL("Open log file", APLogger::LogPath.string().c_str());
            }

            ImGui::InputTextMultiline("##APLoggerMulti", (char*)APLogLocal.c_str(), sizeof(APLogLocal),
                                      ImVec2(ImGui::GetContentRegionAvail().x, 150), ImGuiInputTextFlags_ReadOnly);
        }
    }
}
