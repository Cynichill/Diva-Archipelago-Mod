#pragma once
#include "pch.h"

namespace APSettings
{
    void load();
    void save();
    void apply(const toml::table& settings);
    void collect();

    void ImGuiTab();
}
