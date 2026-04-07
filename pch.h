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
#include <Archipelago.h>
#include "APLogger.h"
#include "Helpers.h"
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

// TODO: Relocate
inline void HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
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
