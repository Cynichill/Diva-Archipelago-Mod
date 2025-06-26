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

const std::string ConfigTOML = "config.toml"; // CWD within Init()
const std::string OutputFileName = "mods/ArchipelagoMod/results.json";
const char* DeathLinkInFile = "mods/ArchipelagoMod/death_link_in";
const std::string DeathLinkOutFile = "mods/ArchipelagoMod/death_link_out";

void* DivaScoreTrigger = sigScan(
    "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x48\x89\x7C\x24\x00\x55\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8B\xEC\x48\x83\xEC\x60\x48\x8B\x05\x00\x00\x00\x00\x48\x33\xC4\x48\x89\x45\xF8\x48\x8B\xF9\x80\xB9\x00\x00\x00\x00\x00\x0F\x85\x00\x00\x00\x00",
    "xxxx?xxxx?xxxx?xxxxxxxxxxxxxxxxxxx????xxxxxxxxxxxx?????xx????"
);

// TODO: Meh trigger. Called twice: FAILURE and around Results screen. Differentiate with remaining HP.
void* DivaDeathTrigger = sigScan(
    "\x48\x89\x6C\x24\x18\x48\x89\x74\x24\x20\x41\x56\x48\x83\xEC\x30\x4C\x8B\x35",
    "xxxxxxxxxxxxxxxxxxx"
);

//Difficulty thresholds
float thresholdEasy = 30.0;
float thresholdNormal = 50.0;
float thresholdHard = 60.0;
float thresholdExtreme = 70.0;
float thresholdExExtreme = 70.0;

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

HOOK(int, __fastcall, _PrintResult, DivaScoreTrigger, long long a1) {

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
    if (finalGrade == 2)
    {
        switch (difficulty) 
        {
            case 0: // Easy
                if (percentageEarned < thresholdEasy) 
                {
                    finalGrade = 1;
                }
                break;
            case 1: // Normal
                if (percentageEarned < thresholdNormal) {
                    finalGrade = 1;
                }
                break;
            case 2: // Hard
                if (percentageEarned < thresholdHard) {
                    finalGrade = 1;
                }
                break;
            case 3: // Extreme
                if (percentageEarned < thresholdExtreme) {
                    finalGrade = 1;
                }
                break;
            case 4: // ExExtreme
                if (percentageEarned < thresholdExExtreme) {
                    finalGrade = 1;
                }
                break;
        }
    }

    // Create JSON with all results that will be sent to the bot
    nlohmann::json results = {
        {"pvId", DivaPVId.Id},
        {"pvName", DivaTitle},
        {"pvDifficulty", DivaDif},
        {"scoreGrade", finalGrade},
        {"deathLinked", deathLinked}, 
    };

    // Detach a thread that will be writing the result so the game doesn't hang
    std::cout << "[Archipelago] Writing out results.json" << std::endl;
    std::thread fileWriteThread(writeToFile, results);
    fileWriteThread.detach();

    std::cout << "[Archipelago] DeathLink: deathLinked = " << deathLinked << " -> " << false << std::endl;
    deathLinked = false;

    return original_PrintResult(a1);
};

HOOK(int, __fastcall, _DeathLinkFail, DivaDeathTrigger, long long a1) {
    int HP = *(uint8_t*)DivaGameHPAddress;

    if (!deathLinked) {
        if (HP == 0) { // Results screen 
            deathLinked = true;
            std::ofstream outputFile(DeathLinkOutFile);
            if (outputFile.is_open()) {
                outputFile.close();
                std::cout << "[Archipelago] DeathLink > Sending death_link_out" << std::endl;
            }
            else {
                std::cout << "[Archipelago] DeathLink > Failed to send death_link_out" << std::endl;
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
    bool exists = std::filesystem::exists(DeathLinkInFile);

    if (exists && !deathLinked) {
        std::cout << "[Archipelago] DeathLink < death_link_in exists" << std::endl;
        
        int HP = *(uint8_t*)DivaGameHPAddress;
        int deathLinkHit = (255 * deathLinkPercent) / 100 + 1;
        HP = std::clamp(HP - deathLinkHit, 0, 255);
        std::cout << "[Archipelago] DeathLink < Drop HP by " << deathLinkHit << " (" << deathLinkPercent << "%) total, result: " << HP << std::endl;

        WRITE_MEMORY(DivaGameHPAddress, uint8_t, HP);

        if (HP == 0)
            deathLinked = true;

        remove(DeathLinkInFile);
    }

    return original_GameplayLoopTrigger(a1);
}

extern "C"
{
    void __declspec(dllexport) Init()
    {
        INSTALL_HOOK(_PrintResult);
        INSTALL_HOOK(_DeathLinkFail);
        INSTALL_HOOK(_GameplayLoopTrigger);

        // TODO: Relocate
        try {
            std::ifstream file(ConfigTOML); // CWD is the mod folder within Init
            if (!file.is_open()) {
                std::cout << "[Archipelago] Error opening config file: " << ConfigTOML << std::endl;
                return;
            }

            auto data = toml::parse(file);
            std::string deathlink_percent = data["deathlink_percent"].value_or("100");
            std::cout << "[Archipelago] Config deathlink_percent: " << deathlink_percent << std::endl;
            
            deathLinkPercent = std::clamp(std::stoi(deathlink_percent), 0, 100);
            std::cout << "[Archipelago] Final deathlink_percent: " << deathLinkPercent << std::endl;
        }
        catch (const std::exception& e) {
            std::cout << "[Archipelago] Error parsing TOML file: " << e.what() << std::endl;
        }
    }
}
