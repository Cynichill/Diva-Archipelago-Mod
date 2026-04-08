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
	extern int death_link_amnesty;
	extern int death_link_amnesty_count;

	void config(toml::v3::ex::parse_result& data);
	void reset();
	void check_fail();
	void run(bool);
	void runAmnesty(); // "Send death", but after checking amnesty.
	void prog_hp_update();
	void prog_hp_reset();
	void setHP(uint8_t);

	void ImGuiTab();
};
