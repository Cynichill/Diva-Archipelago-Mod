#pragma once
#include "pch.h"

namespace APLogger
{
	extern bool logToFile;
	const extern std::filesystem::path LogPath;

	void print(const char* const fmt, ...);
	void ImGuiTab();
}
