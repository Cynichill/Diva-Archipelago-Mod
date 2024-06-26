#include "pch.h"
#include "Helpers.h"
#include <SigScan.h>
#include <detours.h>
#include <thread>
#include <string>
#include <fstream>
#include <nlohmann/json.hpp>
#include "Diva.h"

// MegaMix+ addresses

const uint64_t DivaCurrentPVTitleAddress = 0x00000001412EF228;
const uint64_t DivaCurrentPVIdAddress = 0x00000001412C2340;
const uint64_t DivaScoreGradeAddress = 0x00000001416E2D00;
const uint64_t DivaScoreCompletionRateAddress = 0x00000001412EF634;

// Non-SongLimitPatch 1.02
const uint64_t DivaCurrentPVDifficultyAddress = 0x00000001412B634C;

// SongLimitPatch 1.02 ONLY
//const uint64_t DivaCurrentPVDifficultyAddress = 0x00000001423157AC;

const std::string OutputFileName = "mods/ArchipelagoMod/results.json";

bool consoleEnabled = true;

void* DivaScoreTrigger = sigScan(
    "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x48\x89\x7C\x24\x00\x55\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8B\xEC\x48\x83\xEC\x60\x48\x8B\x05\x00\x00\x00\x00\x48\x33\xC4\x48\x89\x45\xF8\x48\x8B\xF9\x80\xB9\x00\x00\x00\x00\x00\x0F\x85\x00\x00\x00\x00",
    "xxxx?xxxx?xxxx?xxxxxxxxxxxxxxxxxxx????xxxxxxxxxxxx?????xx????"
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

HOOK(int, __fastcall, _PrintResult, DivaScoreTrigger, int a1) {

    std::string& DivaTitle = *(std::string*)DivaCurrentPVTitleAddress;
    DIVA_PV_ID DivaPVId = *(DIVA_PV_ID*)DivaCurrentPVIdAddress;
    DIVA_DIFFICULTY DivaDif = *(_DIVA_DIFFICULTY*)DivaCurrentPVDifficultyAddress;
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
    };

    // Detach a thread that will be writing the result so the game doesn't hang
    std::thread fileWriteThread(writeToFile, results);
    fileWriteThread.detach();

    return original_PrintResult(a1);
};

extern "C"
{
    void __declspec(dllexport) Init()
    {
        INSTALL_HOOK(_PrintResult);
    }
}
