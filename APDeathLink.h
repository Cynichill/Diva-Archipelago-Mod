#pragma once
#include "pch.h"
#include <chrono>
#include <stdint.h>


namespace fs = std::filesystem;
namespace APDeathLink
{
	extern bool safetyExpired;
	extern int HPreceived;
	extern int HPtemp;
	extern int HPnumerator;
	extern int HPdenominator;
	extern bool deathLinked;

	void config(toml::v3::ex::parse_result& data);
	void reset();
	void check_fail();
	void run(bool);
	void prog_hp_update();
	void prog_hp_reset();
	void setHP(uint8_t);

	void ImGuiTab();
};
