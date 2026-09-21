#pragma once
#include <chrono>
#include <random>
#include <stdint.h>
#include "pch.h"

namespace fs = std::filesystem;

namespace APTraps
{
	enum struct TrapID : int64_t {
		None = 0,
		Random = 1, // TODO: Client specific Trap ID. Use to roll valid native traps.

		// Datapackage's Trap IDs begin at 30. Up to that can be used internally.
		Hidden = 30,
		Sudden = 31,
		//HiSpeed = 32,
		Slow = 33,
		Stutter = 34,
		Icon = 35,
		PSP = 36, // Changes actual resolution, minor flicker (window resize) on some recording software but should be seamless for the player.
		SFX = 37,
	};

	extern bool& devMode;
	extern std::mt19937 mt;
	extern float trapDuration;
	extern bool isSudden;
	extern bool isHidden;
	extern bool trap_link;
	extern std::vector<std::string> trap_link_tags;

	void config(const toml::table& settings);
	void save(toml::table& settings);

	int reset();
	void runFrame(); // Ran OnFrame for traps that need to run when not in game, usually to disable themselves. Future hook?
	void run(); // Ran during gameplay via hook to expire traps.

	bool canRecv(const int64_t itemID);
	void trapRecv(const int64_t itemID, const bool notify);

	float getTrapEndTime(float& timestampTrap);

	void touchSudden();
	void touchSudden(bool force);
	void touchHidden();
	void touchHidden(bool force);
	void linkSend(const std::string& trapName);
	void linkRecv(const std::string& trapName);

	float& getGameTime();

	void ImGuiTab();
}
