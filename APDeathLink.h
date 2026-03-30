#pragma once
#include "pch.h"
#include <chrono>
#include <stdint.h>


namespace fs = std::filesystem;
namespace APDeathLink
{
	extern bool safetyExpired;
	extern int HPdenominator;
	extern bool deathLinked;

	void config(toml::v3::ex::parse_result& data);
	bool exists(const fs::path& in);
	int touch();
	void reset();
	void check_fail();
	void run();
	void prog_hp_update();
	void prog_hp_reset();
};
