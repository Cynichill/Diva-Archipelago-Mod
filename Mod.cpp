#include "APDeathLink.h"
#include "APIDHandler.h"
#include "APReload.h"
#include "APTraps.h"
#include "Diva.h"
#include "pch.h"
#include <detours.h>

namespace fs = std::filesystem;

// Archipelago Mod variables
bool skip_mainmenu = false;

const fs::path LocalPath = fs::current_path();
const fs::path ConfigTOML = "config.toml";
const fs::path OutputFileName = "results.json";

void processConfig() {
    // Move to a class and do not do this on init time

    try {
        std::ifstream file(LocalPath / ConfigTOML); // CWD is the mod folder within Init
        if (!file.is_open()) {
            APLogger::print("Error opening config file: %s\n", ConfigTOML.c_str());
            return;
        }

        auto data = toml::parse(file);

        skip_mainmenu = data["skip_mainmenu"].value_or(true);
        APDeathLink::config(data);
        APTraps::config(data);
        APReload::config(data);
    }
    catch (const std::exception& e) {
        APLogger::print("Error parsing config file: %s\n", e.what());
    }
}

void writeToFile(const nlohmann::json& results) {
    // Write the JSON to a file
    std::ofstream outputFile(LocalPath / OutputFileName);
    if (outputFile.is_open()) {
        outputFile << results.dump(4); // Pretty-print JSON with an indent of 4 spaces
        outputFile.close();
    }
    else {
        APLogger::print("Failed to write out rseults.json\n");
    }
}

HOOK(void, __fastcall, _PvResultsFinalize, 0x14024B800, uint64_t PvPlayData, long long a2)
{
    auto pvName = (std::string*)((char*)PvPlayData + 0x2CEF8);

    // This might be somewhere in PvPlayData without having to call out
    auto PvGameData = (char*)reinterpret_cast<uint64_t(__fastcall*)(void)>(0x14027DD90)();
    int diff[3];
    memcpy(diff, PvGameData, 3 * sizeof(int));

    nlohmann::json results = {
        { "pvId", *(int*)((char*)PvPlayData + 0x10) },
        { "pvName", pvName->c_str() },
        { "pvDifficulty", diff[1] + diff[2] },
        { "scoreGrade", *(int*)((char*)PvPlayData + 0x2D190) },
        { "deathLinked", APDeathLink::deathLinked },
    };

    APLogger::print("Writing out results.json\n%s\n", results.dump().c_str());
    std::thread fileWriteThread(writeToFile, results);
    fileWriteThread.detach();

    original_PvResultsFinalize(PvPlayData, a2);
}

HOOK(void, __fastcall, _GameplayLoopTrigger, 0x140244BA0, long long a1) {
    // AOB: 48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B f9 33 DB E8 E7 91 03 00
    // TODO: Called rapidly during gameplay. A more precise function and name is preferred.

    APDeathLink::run();
    APTraps::run();

    original_GameplayLoopTrigger(a1);
}

HOOK(void**, __fastcall, _GameplayEnd, 0x14023F9A0) {
    // AOB: 48 83 EC 28 BA 08 00 00 00 65 48 8B 04 25 58 00 00 00 48 8B 08 8B 04 0A 39 05 42 0C A2 0C
    // Called right as the gameplay is ending/fading out. Early enough to scrub modifier use. Happens alongside FAILURE too.
    // The intent is to not let traps prevent keeping scores.

    APDeathLink::check_fail();
    APTraps::reset();

    return original_GameplayEnd();
}

HOOK(bool, __fastcall, _ModifierSudden, 0x14024b720, long long a1) {
    return APTraps::isSudden ? true : original_ModifierSudden(a1);
}

HOOK(bool, __fastcall, _ModifierHidden, 0x14024b730, long long a1) {
    return APTraps::isHidden ? true : original_ModifierHidden(a1);
}

HOOK(float, __fastcall, _SafetyDuration, 0x14024a5f0, long long a1) {
    auto time = original_SafetyDuration(a1);

    APDeathLink::safetyExpired = (time <= 0.0f);
    if (APDeathLink::safetyExpired && APDeathLink::HPdenominator > 1)
        return 0.39f;

    return time;
}

HOOK(char**, __fastcall, _ReadDBLine, 0x1404C5950, uint64_t a1, char** pv_db_prop) {
    std::string line(pv_db_prop[0], pv_db_prop[1]);
    char** original = original_ReadDBLine(a1, pv_db_prop);

    if (original != nullptr && **original >= '1' && **original <= '2' && !APIDHandler::check(line))
        **original = '0';

    return original;
}

HOOK(void, __fastcall, _ChangeGameSubState, 0x1527E49E0, int state, int substate) {
    // This is most likely a greedy hook (especially against Debug without sigscanning).
    // If it becomes a problem, directly watching state change bytes is possible from 0x1402C4810()

    static bool skipped = false;

    if (state == 2 && substate == 47 || state == 12 && substate == 5) {
        APTraps::reset();
    }
    else if (state == 0 || state == 3) {
        skipped = false;
        APIDHandler::update();
    }
    else if (state == 9 && substate == 47 || state == 6 && substate == 47) {
        bool reload_was_needed = APIDHandler::reload_needed;
        APIDHandler::reload_needed = false;
        APIDHandler::unlock();

        if (reload_was_needed) {
            APIDHandler::update();

            if (APIDHandler::toggleIDs.size() > 0) {
                APLogger::print("Forcing needed reload (have IDs)\n");
                original_ChangeGameSubState(0, 1);
                return;
            }
        }

        processConfig();

        if (skip_mainmenu && skipped == false) {
            APLogger::print("Skipping main menu (state: %d)\n", state);
            skipped = true;
            original_ChangeGameSubState(2, 47);
            return;
        }
    }

    original_ChangeGameSubState(state, substate);
}

HOOK(void, __fastcall, _cust_null, 0x1405946E0, long long* a1, unsigned int a2, char a3, long long a4) {
    // When entering Customize: Suppress an intermittent nullptr at 0x1405947A7 related to reloading and possibly modules.
    // It should not be handheld this way, but it's better than a game crash?

    if (a1 == nullptr)
        return;

    original_cust_null(a1, a2, a3, a4);
}

HOOK(void, __fastcall, _load_null, 0x1405948E0, long long* a1, unsigned long long a2, unsigned long long a3, unsigned long long a4) {
    // When entering Gameplay: Suppress an intermittent nullptr at 0x1405949D9 related to reloading and possibly modules.
    // It should not be handheld this way, but it's better than a game crash?

    // 3D PVs will have broken/invis modules.
    if (a1 == nullptr)
        return;

    original_load_null(a1, a2, a3, a4);
}

extern "C"
{
    void __declspec(dllexport) OnFrame(/*IDXGISwapChain* swapChain*/)
    {
        APReload::scan();
    }

    void __declspec(dllexport) PreInit()
    {
        // toml++ does not persist comments and most formatting which is intended for players.
        // Save an option at the cost of a dropped file to inform new players about reloading and the config.
        fs::path reload_file = LocalPath / ".reload_warning";

        if (!fs::exists(reload_file)) {
            try {
                // This is a wasteful read, but it "should" only ever happen one time ever
                std::ifstream file(LocalPath / ConfigTOML);
                auto data = toml::parse(file);

                std::wstring msg = L"Press the reload key on the song list to get new songs.\n"
                    "Songs can be cleared on any available difficulty for the same checks.\n"
                    "Configure the reload key and more in the mod's config.toml.\n\n"
                    "Current reload key: " + data["reload_key"].value_or(L"F7");

                int msgboxID = MessageBox(
                    NULL,
                    msg.c_str(),
                    L"Archipelago Mod",
                    MB_OK
                );
            }
            catch (const std::exception& e) {
                APLogger::print("(PreInit) Error parsing config file: %s\n", e.what());
            }

            std::ofstream reload_out(reload_file);
            reload_out.close();
        }
    }

    void __declspec(dllexport) Init()
    {
        INSTALL_HOOK(_PvResultsFinalize);
        INSTALL_HOOK(_GameplayLoopTrigger);
        INSTALL_HOOK(_GameplayEnd);
        INSTALL_HOOK(_ModifierSudden);
        INSTALL_HOOK(_ModifierHidden);
        INSTALL_HOOK(_SafetyDuration);

        INSTALL_HOOK(_ChangeGameSubState);
        INSTALL_HOOK(_ReadDBLine);
        INSTALL_HOOK(_load_null);
        INSTALL_HOOK(_cust_null);
    }
}
