// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here

// Do not sort without testing

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <SigScan.h>
#include <string>
#include <thread>
#include <toml++/toml.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <detours.h>
#include <Archipelago.h>
#include "APLogger.h"
#include "APGUI.h"
#include "Helpers.h"

using namespace nlohmann;

const uint64_t PvPlayData = 0x1412C2330; // Struct eventually(TM)

// ...\Hatsune Miku Project DIVA Mega Mix Plus\mods\ArchipelagoMod
const std::filesystem::path BasePath = std::filesystem::current_path();

// For some potential back (and hopefully not future) compat.
// Songs are spaced out by this amount. This should match the datapackage's item ID of "Love is War [1]".
// The most common issue would be ID changes in the space before it.
inline int AP_ID_FACTOR = 100;

// TODO: Relocate
inline void HelpMarker(const char* desc)
{
    if (!APGUI::inlineTooltips) {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
    }
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

inline void CenterText(std::string text)
{
    float margin = ImGui::GetContentRegionAvail().x / 2;
    float width = ImGui::CalcTextSize(text.c_str()).x / 2;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + margin - width);
}

#endif //PCH_H
