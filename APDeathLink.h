#pragma once
#include "pch.h"
#include <chrono>
#include <stdint.h>

namespace APDeathLink
{
	extern bool safetyExpired;
	extern int HPreceived;
	extern int HPtemp;
	extern int HPnumerator;
	extern int HPdenominator;
	extern bool deathLinked;
	extern bool death_link;
	extern bool death_link_self;
	extern int death_link_amnesty;
	extern int death_link_amnesty_count;
	extern bool auto_retry; // True: queue a song reset if a DL would kill

	void resetSong();

	void config(const toml::table& settings);
	void save(toml::table& settings);
	void reset();
	void check_fail();
	void run(bool received);
	void runAmnesty(); // "Send death", but after checking amnesty.
	void recvHP();
	void prog_hp_update();
	void prog_hp_reset();
	void setHP(int HP);

	void ImGuiTab();
};
