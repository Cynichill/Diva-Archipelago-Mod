#include "APClient.h"
#include "APLogger.h"
#include <stdarg.h>

namespace APLogger
{
    bool log_to_file = false;
    std::string APLogLocal;

    std::ofstream APLog;
    const std::filesystem::path LogPath = std::filesystem::current_path() / "log.txt";

    void config(const toml::table& settings)
    {
        toml::table section;
        if (settings.contains("logger") && settings["logger"].is_table())
            section = *settings["logger"].as_table();

        log_to_file = section["log_to_file"].value_or(false);
    }

    void save(toml::table& settings)
    {
        toml::table config;
        config.insert("log_to_file", log_to_file);

        settings.insert("logger", config);
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

        if (log_to_file) {
            APLog.open(LogPath, std::ofstream::out | std::ofstream::app);

            if (APLog)
                APLog.write(line, strlen(line));
        }
    }

    // Previously considered for a tab, made a collapsable header instead
    void ImGuiTab()
    {
        /*if (!APClient::devMode)
            return;*/

        if (!ImGui::CollapsingHeader("Logging")) {
            // TODO: Check write perms?
            ImGui::Checkbox("Log to file", &log_to_file);
            if (log_to_file) {
                ImGui::SameLine();
                ImGui::TextLinkOpenURL("Open log file", APLogger::LogPath.string().c_str());
            }

            ImGui::InputTextMultiline("##APLoggerMulti", (char*)APLogLocal.c_str(), sizeof(APLogLocal),
                                      ImVec2(ImGui::GetContentRegionAvail().x, 150), ImGuiInputTextFlags_ReadOnly);
        }
    }
}
