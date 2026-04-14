#pragma once
#include "pch.h"
#include <filesystem>

namespace APLogger
{
	extern bool logToFile;
	const extern std::filesystem::path LogPath;

	void print(const char* const fmt, ...);
	void ImGuiTab();
}
