#pragma once
#include "pch.h"
#include "virtualKey.h"
#include <algorithm>
#include <thread>

namespace APReload
{
	extern std::string reloadVal;
	extern int reloadDelay;
	extern bool skipMainMenu;

	void config(const toml::table& settings);
	void save(toml::table& settings);
	void scan();
	void run();
	void sleepStartup();
	void ChangeGameState(int32_t state);
	void ChangeGameSubState(int32_t state, int32_t substate);

	void ImGuiTab();
}
