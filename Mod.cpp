#include "APDeathLink.h"
#include "APIDHandler.h"
#include "APTraps.h"
#include "Diva.h"
#include "Helpers.h"
#include "pch.h"
#include <detours.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <SigScan.h>
#include <string>
#include <thread>
#include <toml++/toml.h>

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

APIDHandler IDHandler;
APDeathLink DeathLink;
APTraps Traps;

const std::string ConfigTOML = "config.toml"; // CWD within Init()
const std::string OutputFileName = "mods/ArchipelagoMod/results.json";

// The original sigscan from ScoreDiva for MMUI (may have previously worked with FTUI?)
void* MMUIScoreTrigger = sigScan(
    "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x48\x89\x7C\x24\x00\x55\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8B\xEC\x48\x83\xEC\x60\x48\x8B\x05\x00\x00\x00\x00\x48\x33\xC4\x48\x89\x45\xF8\x48\x8B\xF9\x80\xB9\x00\x00\x00\x00\x00\x0F\x85\x00\x00\x00\x00",
    "xxxx?xxxx?xxxx?xxxxxxxxxxxxxxxxxxx????xxxxxxxxxxxx?????xx????"
);

// Difficulty percentage thresholds
float thresholds[5] = { 30.0, 50.0, 60.0, 70.0, 70.0 };

void writeToFile(const nlohmann::json& results) {

    // Write the JSON to a file
    std::ofstream outputFile(OutputFileName);
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
    std::cout << "[Archipelago] Writing out results.json" << std::endl << results << std::endl;
    std::thread fileWriteThread(writeToFile, results);
    fileWriteThread.detach();
    
    DeathLink.reset();
    IDHandler.update();
}

HOOK(int, __fastcall, _FTUIResult, 0x140237F30, long long a1) {
    // AOB: 48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 48 8D AC 24 40 FF FF FF 48 81 EC C0 01 00 00 48 8B 05 12 44 B6 00
    // Can definitely be better. Not quite the function, mostly AET related, but called on results in FTUI and not MMUI.

    std::cout << "[Archipelago] FTUIResult a1: " << a1 << std::endl;
    processResults();
    return original_FTUIResult(a1);
}

HOOK(int, __fastcall, _MMUIResult, MMUIScoreTrigger, long long a1) {
    std::cout << "[Archipelago] MMUIResult a1: " << a1 << std::endl;
    processResults();
    return original_MMUIResult(a1);
};

HOOK(int, __fastcall, _DeathLinkFail, 0x1514F0ED0, long long a1) {
    DeathLink.fail();

    return original_DeathLinkFail(a1);
};

HOOK(int, __fastcall, _GameplayLoopTrigger, 0x140244BA0, long long a1) {
    // AOB: 48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B f9 33 DB E8 E7 91 03 00
    // TODO: Called rapidly during gameplay. A more precise function and name is preferred.

    DeathLink.run();
    Traps.run();

    return original_GameplayLoopTrigger(a1);
}

HOOK(void, __fastcall, _GameplayEnd, 0x14023F9A0) {
    // AOB: 48 83 EC 28 BA 08 00 00 00 65 48 8B 04 25 58 00 00 00 48 8B 08 8B 04 0A 39 05 42 0C A2 0C
    // Called right as the gameplay is ending/fading out. Early enough to scrub modifier use. Happens alongside FAILURE too.
    // The intent is to not let traps prevent keeping scores.

    Traps.reset();
    
    return original_GameplayEnd();
}

HOOK(long long, __fastcall, _ReadDBs, 0x1404c5950, int a1, long long a2) {
    // AOB: 48 83 ec 38 80 39 00
    // Called on re/load. Super scuffed. Filter songs by ID by reporting 0 for the difficulty lengths.

    std::string line = *(char**)a2;

    if (!IDHandler.check(line))
        return 0;

    return original_ReadDBs(a1, a2); // Default: Enable
}

void processConfig() {
    // Move to a class and do not do this on init time

    try {
        std::ifstream file(ConfigTOML); // CWD is the mod folder within Init
        if (!file.is_open()) {
            std::cout << "[Archipelago] Error opening config file: " << ConfigTOML << std::endl;
            return;
        }

        auto data = toml::parse(file);

        DeathLink.config(data);
        Traps.config(data);
    }
    catch (const std::exception& e) {
        std::cout << "[Archipelago] Error parsing TOML file: " << e.what() << std::endl;
    }
}

extern "C"
{
    void __declspec(dllexport) Init()
    {
        INSTALL_HOOK(_MMUIResult);
        INSTALL_HOOK(_FTUIResult);
        INSTALL_HOOK(_DeathLinkFail);
        INSTALL_HOOK(_GameplayLoopTrigger);
        INSTALL_HOOK(_GameplayEnd);
        INSTALL_HOOK(_ReadDBs);


        processConfig();
    }
}
