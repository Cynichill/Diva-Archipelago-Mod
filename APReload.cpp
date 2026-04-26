#include "APReload.h"
#include "virtualKey.h"
#include <algorithm>
#include <thread>

namespace APReload
{
    HWND hGameWindow;
    std::string reloadVal;
    int reloadKeyCode;
    int reloadDelay = 10;
    bool skipMainMenu = false;

    void config(const toml::table& settings)
    {
        toml::table section;
        if (settings.contains("reload") && settings["reload"].is_table())
            section = *settings["reload"].as_table();

        reloadVal = section["key"].value_or<std::string>("F7");
        reloadVal = reloadVal.empty() ? "F7" : reloadVal;
        reloadKeyCode = GetReloadKeyCode(reloadVal);

        APLogger::print("reload key: %s (0x%x)\n", reloadVal.c_str(), static_cast<int>(reloadKeyCode));

        reloadDelay = std::clamp(section["delay"].value_or(10), 1, 10);
        APLogger::print("reload delay: %ims\n", reloadDelay * 100);

        skipMainMenu = section["skip_main_menu"].value_or(false);
        APLogger::print("reload skip_main_menu: %i\n", skipMainMenu);

        // DATA_TEST patch thanks to Debug mod: samyuu, nastys, vixen256, korenkonder, skyth
        WRITE_MEMORY(0x140441153, uint8_t, 0xE9, 0x1E, 0x00, 0x00, 0x00, 0x00);

        if (!hGameWindow)
            hGameWindow = GetActiveWindow();
    }

    void save(toml::table& settings)
    {
        toml::table config;
        config.insert("key", reloadVal);
        config.insert("delay", reloadDelay);
        config.insert("skip_main_menu", skipMainMenu);

        settings.insert("reload", config);
    }

    void scan()
    {
        if (!hGameWindow || GetForegroundWindow() != hGameWindow)
            return;

        static bool pressed = false;

        bool wasPressed = pressed;
        pressed = (GetAsyncKeyState(reloadKeyCode) & 0x8000) != 0;

        if (pressed && !wasPressed)
            run();
    }

    void run()
    {
        int* state = (int*)0x14CC61078;
        int* substate = (int*)0x14CC61094;

        if (*state == 2 && *substate == 7 || *state == 0 /*|| *state == 3*/ || *state == 7) {
            // Init, test, and Cust. In game including FTUI, MV, practice, and results.
            // state 7: reproducible infinite load/crash when reloading on Cust screen with 4 or more charas.
            //          only covers main menu -> cust, not song list -> cust
            APLogger::print("Reloading blocked for state %i/%i\n", *state, *substate);
            return;
        }

        APLogger::print("Reload < %i/%i\n", *state, *substate);

        ChangeGameState(3);

        std::thread startup(sleepStartup);
        startup.detach();
    }

    void sleepStartup()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(reloadDelay * 100));
        ChangeGameSubState(0, 1);
    }

    void ChangeGameState(int32_t state)
    {
        APLogger::print("Reload > %i\n", state);
        auto _ChangeGameState = reinterpret_cast<uint64_t(__fastcall*)(int32_t)>(0x1402C4BB0);
        _ChangeGameState(state);
    }

    void ChangeGameSubState(int32_t state, int32_t substate)
    {
        APLogger::print("Reload > %i/%i\n", state, substate);
        auto _ChangeGameSubState = reinterpret_cast<uint64_t(__fastcall*)(int32_t, int32_t)>(0x1527E49E0);
        _ChangeGameSubState(state, substate);
    }

    void ImGuiTab()
    {
        if (ImGui::CollapsingHeader("Reloading")) {
            if (ImGui::Button("Reload game"))
                APReload::run();

            ImGui::SameLine();
            if (ImGui::Button("Force"))
                APReload::ChangeGameState(1);

            ImGui::SameLine();
            ImGui::Text("Reload key: %s", APReload::reloadVal.c_str());
            ImGui::SameLine();
            HelpMarker("Can only be changed from settings file.");


            ImGui::SliderInt("Reload delay", &APReload::reloadDelay, 1, 10);
            ImGui::SameLine();
            HelpMarker("How long to wait for the reload.\nLower is faster but may break.\nBest with DivaModLoader PR #36");

            ImGui::Checkbox("Skip main menu", &APReload::skipMainMenu);
            ImGui::SameLine();
            HelpMarker("Skip the main menu after title screen.\nCombine with IntroPatch to skip to song select.");
        }
    }
}
