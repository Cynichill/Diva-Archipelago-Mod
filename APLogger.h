#pragma once
#include "pch.h"

namespace APLogger
{
	extern bool logToFile;
	const extern std::filesystem::path LogPath;

	void config(toml::v3::ex::parse_result& data);
	void print(const char* const fmt, ...);
	void ImGuiTab();
}
