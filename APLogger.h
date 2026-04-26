#pragma once
#include "pch.h"

namespace APLogger
{
	extern bool log_to_file;
	const extern std::filesystem::path LogPath;

	void config(const toml::table& settings);
	void save(toml::table& settings);

	void print(const char* const fmt, ...);
	void ImGuiTab();
}
