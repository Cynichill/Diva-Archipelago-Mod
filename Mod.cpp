#include "pch.h"
#include "Helpers.h"
#include <SigScan.h>
#include <detours.h>
#include <thread>
#include <string>
#include <fstream>
#include <nlohmann/json.hpp>
#include "Diva.h"
#include <toml++/toml.h>
#include <iostream>
#include <chrono>
#include <random>

// MegaMix+ addresses
const uint64_t DivaCurrentPVTitleAddress = 0x00000001412EF228;
const uint64_t DivaCurrentPVIdAddress = 0x00000001412C2340;
const uint64_t DivaScoreGradeAddress = 0x00000001416E2D00;
const uint64_t DivaScoreCompletionRateAddress = 0x00000001412EF634;

//const uint64_t DivaCurrentPVDifficultyAddress = 0x00000001412B634C; // Non-SongLimitPatch 1.02
//const uint64_t DivaCurrentPVDifficultyAddress = 0x00000001423157AC; // SongLimitPatch 1.02 ONLY
const uint64_t DivaCurrentPVDifficultyBaseAddress = 0x0000000140DAE934;
const uint64_t DivaCurrentPVDifficultyExtraAddress = 0x0000000140DAE938;

// Active gameplay addresses
const uint64_t DivaGameHPAddress = 0x00000001412EF564;
const uint64_t DivaGameModifierAddress = 0x00000001412EF450;
const uint64_t DivaGameIconDisplayAddress = 0x00000001412B6374;

// Archipelago Mod variables
bool consoleEnabled = true;

bool deathLinked = false;
int deathLinkPercent = 100;
int deathLinkSafetySeconds = 10; // Seconds after receiving a DL to avoid chain reaction DLs.
std::chrono::steady_clock::time_point deathLinkTimestamp;

int trapDuration = 10;
std::chrono::steady_clock::time_point trapTimestamp;

const std::string ConfigTOML = "config.toml"; // CWD within Init()
const std::string OutputFileName = "mods/ArchipelagoMod/results.json";

const char* DeathLinkInFile = "mods/ArchipelagoMod/death_link_in";
const std::string DeathLinkOutFile = "mods/ArchipelagoMod/death_link_out";

const char* TrapSuddenInFile = "mods/ArchipelagoMod/sudden";
const char* TrapHiddenInFile = "mods/ArchipelagoMod/hidden";
const char* TrapIconInFile = "mods/ArchipelagoMod/icontrap";
uint8_t untrapOriginalIcons = 39;

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> nextIconGenerator(0, 4);

// The original sigscan from ScoreDiva for MMUI (may have previously worked with FTUI?)
void* MMUIScoreTrigger = sigScan(
    "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x48\x89\x7C\x24\x00\x55\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8B\xEC\x48\x83\xEC\x60\x48\x8B\x05\x00\x00\x00\x00\x48\x33\xC4\x48\x89\x45\xF8\x48\x8B\xF9\x80\xB9\x00\x00\x00\x00\x00\x0F\x85\x00\x00\x00\x00",
    "xxxx?xxxx?xxxx?xxxxxxxxxxxxxxxxxxx????xxxxxxxxxxxx?????xx????"
);
// Can definitely be better. Not quite the function, mostly AET related, but called on results in FTUI and not MMUI.
const uint64_t FTUIScoreTrigger = 0x140237F30;

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
        {"deathLinked", deathLinked},
    };

    // Detach a thread that will be writing the result so the game doesn't hang
    std::cout << "[Archipelago] Writing out results.json" << std::endl << results << std::endl;
    std::thread fileWriteThread(writeToFile, results);
    fileWriteThread.detach();

    std::cout << "[Archipelago] DeathLink: deathLinked = " << deathLinked << " -> " << false << std::endl;
    deathLinked = false;
}

HOOK(int, __fastcall, _FTUIResult, FTUIScoreTrigger, long long a1) {
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
    int HP = *(uint8_t*)DivaGameHPAddress;

    if (!deathLinked) {
        if (HP == 0) { // Results screen 
            deathLinked = true;

            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - deathLinkTimestamp);
            std::cout << "[Archipelago] DeathLink > Seconds since received: " << elapsed.count() << " (safety: " << deathLinkSafetySeconds << "s)" << std::endl;

            if (elapsed.count() < deathLinkSafetySeconds) {
                std::cout << "[Archipelago] DeathLink > In safety window so no death_link_out" << std::endl;
            }
            else {
                std::ofstream outputFile(DeathLinkOutFile);
                if (outputFile.is_open()) {
                    outputFile.close();
                    std::cout << "[Archipelago] DeathLink > Sending death_link_out" << std::endl;
                }
                else {
                    std::cout << "[Archipelago] DeathLink > Failed to send death_link_out" << std::endl;
                }
            }
        }
        else {
            std::cout << "[Archipelago] DeathLink > Called with " << HP << " HP != 0, not sending death_link_out" << std::endl;
        }
    }
    else {
        std::cout << "[Archipelago] DeathLink > Currently dying so no death_link_out" << std::endl;
    }

    return original_DeathLinkFail(a1);
};

// TODO: Called rapidly during gameplay. A more precise function and name is preferred.
void* gameplayLoopTrigger = sigScan(
    "\x48\x89\x5c\x24\x10\x48\x89\x74\x24\x18\x57\x48\x83\xec\x20\x48\x8b\xf9\x33\xdb\xe8\xe7\x91\x03\x00",
    "xxxxxxxxxxxxxxxxxxxxxxxxx"    
);

HOOK(int, __fastcall, _GameplayLoopTrigger, gameplayLoopTrigger, long long a1) {
    bool deathlink_exists = std::filesystem::exists(DeathLinkInFile);
    int HP = *(uint8_t*)DivaGameHPAddress;

    if (deathlink_exists && !deathLinked) {
        std::cout << "[Archipelago] DeathLink < death_link_in exists" << std::endl;
        std::cout << "[Archipelago] DeathLink < Updating timestamp from " << deathLinkTimestamp.time_since_epoch().count() << " to ";
        deathLinkTimestamp = std::chrono::steady_clock::now();
        std::cout << deathLinkTimestamp.time_since_epoch().count() << std::endl;

        int deathLinkHit = (255 * deathLinkPercent) / 100 + 1;
        HP = std::clamp(HP - deathLinkHit, 0, 255);
        std::cout << "[Archipelago] DeathLink < Drop HP by " << deathLinkHit << " (" << deathLinkPercent << "%) total, result: " << HP << std::endl;

        WRITE_MEMORY(DivaGameHPAddress, uint8_t, static_cast<uint8_t>(HP));

        if (HP == 0)
            deathLinked = true;

        remove(DeathLinkInFile);
    } else if (HP > 0 && deathLinked) {
        // deathLinked reset alternative to results screen.
        deathLinked = false;
    }

    // Do not interfere with modifier mods, even if they have a chance to stack.
    // Need to track if set by AP.
    int currentMod = *(int*)DivaGameModifierAddress;
    if (currentMod <= 3) { 
        bool sudden_exists = std::filesystem::exists(TrapSuddenInFile);
        bool hidden_exists = std::filesystem::exists(TrapHiddenInFile);
        bool icon_exists = std::filesystem::exists(TrapIconInFile);
        bool newTrap = sudden_exists || hidden_exists; // || icon_exists;

        if (sudden_exists) {
            std::cout << "[Archipelago] Trap < Sudden" << std::endl;
            WRITE_MEMORY(DivaGameModifierAddress, uint8_t, DIVA_MODIFIERS::Sudden);
            remove(TrapSuddenInFile);
        }

        if (hidden_exists) {
            std::cout << "[Archipelago] Trap < Hidden" << std::endl;
            WRITE_MEMORY(DivaGameModifierAddress, uint8_t, DIVA_MODIFIERS::Hidden);
            remove(TrapHiddenInFile);
        }

        if (icon_exists) {
            std::cout << "[Archipelago] Trap < Icon" << std::endl;

            // PS 0-3, Arrows 4, NSW 5-8, XBOX 9-12, any can switch in/out of arrows but should be tracked.
            uint8_t curIcon = *(uint8_t*)DivaGameIconDisplayAddress;
            uint8_t nextIcon = nextIconGenerator(gen);

            if (untrapOriginalIcons == 39) {
                std::cout << "[Archipelago] Trap < Original Icon is " << (int)curIcon << std::endl;
                untrapOriginalIcons = curIcon;
            }

            // Catch Arrows, XBOX, and NSW
            if (nextIcon != 4)
                if (curIcon == 4)
                    nextIcon = untrapOriginalIcons;
            else if (curIcon >= 9)
                nextIcon += 9;
            else if (curIcon >= 5)
                nextIcon += 5;

            std::cout << "[Archipelago] Trap < Icons " << (int)curIcon << " -> " << (int)nextIcon << std::endl;
            WRITE_MEMORY(DivaGameIconDisplayAddress, uint8_t, nextIcon);

            remove(TrapIconInFile);
        }

        if (newTrap)
            trapTimestamp = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - trapTimestamp);

        if (currentMod > 0 && trapDuration > 0 && elapsed.count() >= trapDuration) {
            std::cout << "[Archipelago] Trap > Duration expired, unset" << std::endl;
            WRITE_MEMORY(DivaGameModifierAddress, uint8_t, DIVA_MODIFIERS::None);
        }
    }

    return original_GameplayLoopTrigger(a1);
}

HOOK(void, __fastcall, _GameplayEnd, 0x14023F9A0) {
    // Called right as the gameplay is ending/fading out. Early enough to scrub modifier use. Happens alongside FAILURE too.
    // The intent is to not let traps prevent keeping scores.

    int currentMod = *(int*)DivaGameModifierAddress;

    if (currentMod > 0 && currentMod <= 3 /* && MODIFIER_WAS_SET_BY_ARCHIPELAGO */)
        std::cout << "[Archipelago] Unset modifier: " << currentMod << " -> 0" << std::endl;
        WRITE_MEMORY(DivaGameModifierAddress, uint8_t, DIVA_MODIFIERS::None);

    if (untrapOriginalIcons <= 12)
        std::cout << "[Archipelago] Restoring Icons to " << (int)untrapOriginalIcons << std::endl;
        WRITE_MEMORY(DivaGameIconDisplayAddress, uint8_t, untrapOriginalIcons);
        untrapOriginalIcons = 39;
    
    return original_GameplayEnd();
}

void processConfig() {
    try {
        std::ifstream file(ConfigTOML); // CWD is the mod folder within Init
        if (!file.is_open()) {
            std::cout << "[Archipelago] Error opening config file: " << ConfigTOML << std::endl;
            return;
        }

        auto data = toml::parse(file);

        std::string deathlink_percent = data["deathlink_percent"].value_or(std::to_string(deathLinkPercent));
        std::cout << "[Archipelago] Config deathlink_percent: " << deathlink_percent << std::endl;
        deathLinkPercent = std::clamp(std::stoi(deathlink_percent), 0, 100);
        std::cout << "[Archipelago] Final deathlink_percent: " << deathLinkPercent << std::endl;

        std::string trap_duration = data["trap_duration"].value_or(std::to_string(trapDuration));
        std::cout << "[Archipelago] Config trap_duration: " << trap_duration << std::endl;
        trapDuration = std::clamp(std::stoi(trap_duration), 0, 60);
        std::cout << "[Archipelago] Final trap_duration: " << trapDuration << std::endl;
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

        processConfig();
    }
}
