#include "APDeathLink.h"
#include "APIDHandler.h"
#include "APLogger.h"
#include "APTraps.h"
#include "Diva.h"
#include "Helpers.h"
#include "pch.h"
#include <detours.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <SigScan.h>
#include <string>
#include <thread>
#include <toml++/toml.h>

namespace fs = std::filesystem;

// MegaMix+ addresses
const uint64_t DivaCurrentPVTitleAddress = 0x00000001412EF228;
const uint64_t DivaCurrentPVIdAddress = 0x00000001412C2340;
const uint64_t DivaScoreGradeAddress = 0x00000001416E2D00;
const uint64_t DivaScoreCompletionRateAddress = 0x00000001412EF634;

//const uint64_t DivaCurrentPVDifficultyAddress = 0x00000001412B634C; // Non-SongLimitPatch 1.02
//const uint64_t DivaCurrentPVDifficultyAddress = 0x00000001423157AC; // SongLimitPatch 1.02 ONLY
const uint64_t DivaCurrentPVDifficultyBaseAddress = 0x0000000140DAE934;
const uint64_t DivaCurrentPVDifficultyExtraAddress = 0x0000000140DAE938;

// Archipelago Mod variables
bool consoleEnabled = true;
bool skip_mainmenu = false;

APDeathLink DeathLink;
APIDHandler IDHandler;
APTraps Traps;

const fs::path LocalPath = fs::current_path();
const fs::path ConfigTOML = "config.toml";
const fs::path OutputFileName = "results.json";

// Difficulty percentage thresholds
float thresholds[5] = { 30.0, 50.0, 60.0, 70.0, 70.0 };

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
        DeathLink.config(data);
        Traps.config(data);

        // toml++ does not persist comments and most formatting which is intended for players.
        // Save an option at the cost of a file to inform new players about reloading and the config.
        fs::path reload_file = LocalPath / ".reload_warning";
        if (!fs::exists(reload_file)) {
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

            std::ofstream reload_out(reload_file);
            reload_out.close();
        }
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
        if (consoleEnabled)
            printf("Failed to open the file for writing.\n");
    }
}

void processResults() {
    std::string& DivaTitle = *(std::string*)DivaCurrentPVTitleAddress;
    DIVA_PV_ID DivaPVId = *(DIVA_PV_ID*)DivaCurrentPVIdAddress;
    int DivaBaseDiff = *(int*)DivaCurrentPVDifficultyBaseAddress;
    int DivaExtraFlag = *(int*)DivaCurrentPVDifficultyExtraAddress;
    DIVA_DIFFICULTY DivaDif = (_DIVA_DIFFICULTY)(DivaBaseDiff + DivaExtraFlag);
    DIVA_GRADE DivaGrade = *(_DIVA_GRADE*)DivaScoreGradeAddress;
    DIVA_STAT DivaStat = *(DIVA_STAT*)DivaScoreCompletionRateAddress;

    int finalGrade = int(DivaGrade);
    int difficulty = int(DivaDif);
    float percentageEarned = float(DivaStat.CompletionRate);

    //If % earned is less than threshold, fail song
    if (finalGrade == 2 && percentageEarned < thresholds[difficulty])
        finalGrade = 1;

    // Create JSON with all results that will be sent to the bot
    nlohmann::json results = {
        {"pvId", DivaPVId.Id},
        {"pvName", DivaTitle},
        {"pvDifficulty", DivaDif},
        {"scoreGrade", finalGrade},
        {"deathLinked", DeathLink.deathLinked},
    };

    // Detach a thread that will be writing the result so the game doesn't hang
    APLogger::print("Writing out results.json\n%s\n", results.dump().c_str());
    std::thread fileWriteThread(writeToFile, results);
    fileWriteThread.detach();

    DeathLink.reset();
}

HOOK(void, __fastcall, _FTUIResult, 0x140237F30, long long a1) {
    // AOB: 48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 48 8D AC 24 40 FF FF FF 48 81 EC C0 01 00 00 48 8B 05 12 44 B6 00
    // Can definitely be better. Not quite the function, mostly AET related, but called on results in FTUI and not MMUI.

    APLogger::print("FTUI Result\n");
    processResults();
    original_FTUIResult(a1);
}

HOOK(void, __fastcall, _MMUIResult, 0x140649e10, long long a1) {
    // AOB: 48 89 5C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 55 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 60 48 8B 05 ? ? ? ? 48 33 C4 48 89 45 F8 48 8B F9 80 B9 ? ? ? ? ? 0F 85 ? ? ? ?
    APLogger::print("MMUI Result\n");
    processResults();
    original_MMUIResult(a1);
};

HOOK(void, __fastcall, _GameplayLoopTrigger, 0x140244BA0, long long a1) {
    // AOB: 48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B f9 33 DB E8 E7 91 03 00
    // TODO: Called rapidly during gameplay. A more precise function and name is preferred.

    DeathLink.run();
    Traps.run();

    original_GameplayLoopTrigger(a1);
}

HOOK(void**, __fastcall, _GameplayEnd, 0x14023F9A0) {
    // AOB: 48 83 EC 28 BA 08 00 00 00 65 48 8B 04 25 58 00 00 00 48 8B 08 8B 04 0A 39 05 42 0C A2 0C
    // Called right as the gameplay is ending/fading out. Early enough to scrub modifier use. Happens alongside FAILURE too.
    // The intent is to not let traps prevent keeping scores.

    DeathLink.check_fail();
    Traps.reset();

    return original_GameplayEnd();
}

HOOK(bool, __fastcall, _ModifierSudden, 0x14024b720, long long a1) {
    return Traps.isSudden ? true : original_ModifierSudden(a1);
}

HOOK(bool, __fastcall, _ModifierHidden, 0x14024b730, long long a1) {
    return Traps.isHidden ? true : original_ModifierHidden(a1);
}

HOOK(float, __fastcall, _SafetyDuration, 0x14024a5f0, long long a1) {
    auto time = original_SafetyDuration(a1);

    DeathLink.safetyExpired = (time <= 0.0f);
    if (DeathLink.safetyExpired && DeathLink.HPdenominator > 1)
        return 65535.0f; // Surely there's no 18 hour song

    return original_SafetyDuration(a1);
}

HOOK(char**, __fastcall, _ReadDBLine, 0x1404C5950, uint64_t a1, char** pv_db_prop) {
    std::string line(pv_db_prop[0], pv_db_prop[1]);
    char** original = original_ReadDBLine(a1, pv_db_prop);

    if (original != nullptr && **original >= '1' && **original <= '2' && !IDHandler.check(line))
        **original = '0';

    return original;
}

HOOK(void, __fastcall, _ChangeGameSubState, 0x1527E49E0, int state, int substate) {
    // This is most likely a greedy hook (especially against Debug without sigscanning).
    // If it becomes a problem, directly watching state change bytes is possible from 0x1402C4810()

    static bool skipped = false;

    if (state == 2 && substate == 47 || state == 12 && substate == 5) {
        Traps.reset();
    }
    else if (state == 0 || state == 3) {
        skipped = false;
        IDHandler.update();
    }
    else if (state == 9 && substate == 47 || state == 6 && substate == 47) {
        bool reload_was_needed = IDHandler.reload_needed;
        IDHandler.reload_needed = false;
        IDHandler.unlock();

        if (reload_was_needed) {
            APLogger::print("Forcing needed reload\n");
            IDHandler.update();
            original_ChangeGameSubState(0, 1);
            return;
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
    void __declspec(dllexport) Init()
    {
        INSTALL_HOOK(_MMUIResult);
        INSTALL_HOOK(_FTUIResult);
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
