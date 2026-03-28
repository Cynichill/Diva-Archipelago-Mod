#include "APLogger.h"
#include "APReload.h"
#include "Helpers.h"
#include "virtualKey.h"
#include <algorithm>
#include <thread>

void APReload::config(toml::v3::ex::parse_result& data)
{
	reloadVal = data["reload_key"].value_or("F7");
	reloadKeyCode = GetReloadKeyCode(reloadVal);

	APLogger::print("reload_key: %s (0x%x)\n",
					reloadVal.c_str(), static_cast<int>(reloadKeyCode));

	reloadDelay = std::clamp(data["reload_delay"].value_or(10), 1, 10) * 100;
	APLogger::print("reload_delay: %ims\n", reloadDelay);

	// DATA_TEST patch thanks to Debug mod: samyuu, nastys, vixen256, korenkonder, skyth
	WRITE_MEMORY(0x140441153, uint8_t, 0xE9, 0x1E, 0x00, 0x00, 0x00, 0x00);
	
	if (!hGameWindow)
		hGameWindow = GetActiveWindow();
}

void APReload::scan()
{
	if (GetForegroundWindow() != hGameWindow)
		return;

	static bool pressed = false;

	bool wasPressed = pressed;
	pressed = (GetAsyncKeyState(reloadKeyCode) & 0x8000) != 0;

	if (pressed && !wasPressed) {
		int* state = (int*)0x14CC61078;
		int* substate = (int*)0x14CC61094;

		if (*state == 2 && *substate == 7 || *state == 0 || *state == 3 || *state == 7) {
			// Init, test, and Cust. In game including FTUI, MV, practice, and results.
			// state 7: reproducible infinite load/crash when reloading on Cust screen with 4 or more charas.
			//          only covers main menu -> cust, not song list -> cust
			APLogger::print("Reloading blocked for state %i/%i\n", *state, *substate);
			return;
		}

		APLogger::print("Reload < %i/%i\n", *state, *substate);

		ChangeGameState(3);

		std::thread startup(&APReload::sleepStartup, this);
		startup.detach();
	}
}

void APReload::sleepStartup()
{
	// It may be better/possible to poll the state (DATA_TEST) and react immediately.
	std::this_thread::sleep_for(std::chrono::milliseconds(reloadDelay));
	ChangeGameSubState(0, 1);
}

void APReload::ChangeGameState(int32_t state)
{
	APLogger::print("Reload > %i\n", state);
	auto _ChangeGameState = reinterpret_cast<uint64_t(__fastcall*)(int32_t)>(0x1402C4BB0);
	_ChangeGameState(state);
}

void APReload::ChangeGameSubState(int32_t state, int32_t substate)
{
	APLogger::print("Reload > %i/%i\n", state, substate);
	auto _ChangeGameSubState = reinterpret_cast<uint64_t(__fastcall*)(int32_t, int32_t)>(0x1527E49E0);
	_ChangeGameSubState(state, substate);
}