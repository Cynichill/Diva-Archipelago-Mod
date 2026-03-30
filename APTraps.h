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

	void config(toml::v3::ex::parse_result& data);

	int reset();
	void resetIcon();
	void run();

	bool exists(const fs::path& in);
	uint64_t getGameControlConfig();
	uint64_t getIconAddress();

	uint8_t getCurrentIcon();
	void rollIcon();
}
