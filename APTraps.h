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

		// Datapackage's Trap IDs begin at 30. Up to that can be used for whatever.
		Hidden = 30,
		Sudden = 31,
		//HiSpeed = 32,
		Slow = 33,
		Stutter = 34,
		Icon = 35,
		PSP = 36, // Changes actual resolution, minor flicker (window resize) on some recording software but should be seamless for the player.
	};

	struct Resolution {
		uintptr_t* path = reinterpret_cast<uint64_t*>(0x141148218);
		int width = 0;
		int height = 0;
		int offsetX = 0;
		int offsetY = 0;

		void clear() {
			*this = {};
		}

		void update() {
			width = *(int*)(*(path)+0x40);
			height = *(int*)(*(path)+0x44);
			offsetX = *(int*)(*(path)+0x48);
			offsetY = *(int*)(*(path)+0x4c);
		}
	};

	extern bool isSudden;
	extern bool isHidden;
	extern bool isSlow;
	extern bool trap_link;

	void config(const toml::table& settings);
	void save(toml::table& settings);

	int reset();
	void resetIcon();
	void runFrame(); // Ran OnFrame for traps that need to run when not in game, usually to disable themselves. Future hook?
	void run(); // Ran during gameplay via hook to expire traps.
	void runSlow();
	void runPSP();

	bool canRecv(const int64_t itemID);
	void trapRecv(const int64_t itemID, const bool notify);

	void menuOpened();
	void touchSudden();
	void touchSudden(bool force);
	void touchHidden();
	void touchHidden(bool force);
	void touchStutter();
	void touchIcon();
	void touchSlow();
	void touchPSP();
	void linkSend(const std::string& trapName);
	void linkRecv(const std::string& trapName);

	uint64_t getGameControlConfig();
	uint64_t getIconAddress();

	int getCurrentIcon();
	void rollIcon();

	void ImGuiTab();
}
