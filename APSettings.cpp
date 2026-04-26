#include "APClient.h"
#include "APDeathLink.h"
#include "APGUI.h"
#include "APReload.h"
#include "APSettings.h"
#include "APTraps.h"

namespace APSettings
{
    namespace fs = std::filesystem;
    auto LocalPath = fs::current_path();
    const fs::path SettingsTOML = LocalPath / "settings.toml";

    void load()
    {
        toml::table settings;

        try {
            auto parse = toml::parse_file(SettingsTOML.u8string());
            settings = *parse.as_table();
        }
        catch (const std::exception& e) {
            APLogger::print("Error parsing settings file: %s\n", e.what());
        }

        apply(settings);
    }

    void save()
    {
        collect();
    }

    void apply(const toml::table &settings)
    {
        APClient::config(settings);
        APDeathLink::config(settings);
        APGUI::config(settings);
        APLogger::config(settings);
        APReload::config(settings);
        APTraps::config(settings);
    }

    void collect()
    {
        toml::table settings;

        APClient::save(settings);
        APDeathLink::save(settings);
        APGUI::save(settings);
        APLogger::save(settings);
        APReload::save(settings);
        APTraps::save(settings);

        std::ofstream settingsFile(SettingsTOML);
        if (settingsFile.is_open()) {
            settingsFile << "# It is recommend to make changes and save settings within the game.\n"
                            "# Invalid or missing settings here will use their defaults.\n\n";
            settingsFile << toml::toml_formatter{ settings, toml::format_flags::relaxed_float_precision };
        }
    }

    void ImGuiTab()
    {
        if (ImGui::Button("Save")) APSettings::save();
        ImGui::SameLine();
        if (ImGui::Button("Reload")) APSettings::load();
        ImGui::SameLine();
        ImGui::TextLinkOpenURL("Open settings.toml", SettingsTOML.u8string().c_str());
    }
}