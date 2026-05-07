#pragma once
#include <chrono>
#include <random>
#include <stdint.h>
#include "pch.h"

namespace fs = std::filesystem;

namespace APTraps
{
	extern bool isSudden;
	extern bool isHidden;

	void config(const toml::table& settings);
	void save(toml::table& settings);

	int reset();
	void resetIcon();
	void run();

	void touchSudden();
	void touchHidden();
	void touchIcon();

	uint64_t getGameControlConfig();
	uint64_t getIconAddress();

	uint8_t getCurrentIcon();
	void rollIcon();

	void ImGuiTab();
}
