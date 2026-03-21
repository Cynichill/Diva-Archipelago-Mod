#include "deck.h"
#include "MemSigScan.h"
#include "pch.h"
#include "virtualKey.h"
#include <chrono>
#include <filesystem>
#include <thread>
#include "APLogger.h"

using namespace GameStates;

namespace EnableDebugMode_AP
{
	enum class PluginState { RegularGame, WaitingToSelectDataTest, InDataTest };

	static PluginState CurrentState = PluginState::RegularGame;
	static bool IsMainWindowFocused = false;

	static auto Original_ChangeGameState = reinterpret_cast<void(__fastcall*)(GameState)>(0x00000001402C46F0);
	static auto Original_ChangeGameSubState = reinterpret_cast<void(__fastcall*)(GameState, GameSubState)>(0x000000015241DA80);

	static std::chrono::steady_clock::time_point conditionMetTimePoint;

	static bool IsKeyPressed(u8 keyCode) { return (::GetAsyncKeyState(keyCode) & 0x8000) != 0; }

	// Reload key stuff
	std::filesystem::path LocalPath;
	unsigned int reloadDelay = 1000;
	std::string reloadVal = "F7";
	u8 reloadKeyCode = 0x76;
	bool reloadKeyWasDown = false;
	bool waitingForCommand = false;
	std::chrono::steady_clock::time_point delayStart;

	void configReload(const std::filesystem::path& filename) {
		std::ifstream file(filename);
		if (!file.is_open()) {
			APLogger::print("Error opening file: %s\n", filename.string().c_str());
		}

		try {
			auto data = toml::parse(file);

			reloadVal = data["reload_key"].value_or("F7");
			reloadKeyCode = GetReloadKeyCode(reloadVal);

			APLogger::print("Reload value: %s (0x%x)\n",
							reloadVal.c_str(), static_cast<int>(reloadKeyCode));

			reloadDelay = std::clamp(data["reload_delay"].value_or(10), 1, 10) * 100;
			APLogger::print("Reload delay: %ims\n", reloadDelay);

		}
		catch (const std::exception& e) {
			APLogger::print("Error parsing TOML file: %s\n", e.what());
		}
	}

	static void pluginLoop()
	{
		bool keyDown = IsKeyPressed(reloadKeyCode);
		int* state = (int*)0x14CC61078;
		int* substate = (int*)0x14CC61094;

		if (IsMainWindowFocused && keyDown && !reloadKeyWasDown)
		{
			if (*state == 2 && *substate == 7 || *state == 0 || *state == 3) {
				// In game including FTUI, MV, practice, and results. Init and test.
				// || *state == 7, reproducible when reloading on Cust screen with 4 or more charas.
				reloadKeyWasDown = keyDown;
				APLogger::print("Reloading blocked for state %i/%i\n", *state, *substate);
				return;
			}

			APLogger::print("Reloading from state %i/%i\n", *state, *substate);

			Original_ChangeGameState(GameState::DATA_TEST);
			CurrentState = PluginState::WaitingToSelectDataTest;

			delayStart = std::chrono::steady_clock::now();
			waitingForCommand = true;
		}

		reloadKeyWasDown = keyDown;

		// Instead of waiting reloadDay it may be fine to poll the states and change immediately.
		if (waitingForCommand)
		{
			auto elapsed = std::chrono::steady_clock::now() - delayStart;

			if (elapsed > std::chrono::milliseconds(reloadDelay))
			{
				DLLWindowProc(nullptr, WM_COMMAND, 0, 0);
				waitingForCommand = false;
			}
		}
	}

	static void(__fastcall* Original_EngineUpdateTick)(void*) = nullptr;
	static void __fastcall Hooked_EngineUpdateTick(void* arg0)
	{
		pluginLoop();

		return Original_EngineUpdateTick(arg0);
	}

	static LRESULT(__stdcall* Original_MainWindowWndProcA)(HWND, UINT, WPARAM, LPARAM) = nullptr;
	static LRESULT __stdcall Hooked_MainWindowWndProcA(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_ACTIVATE:
		{
			IsMainWindowFocused = (wParam != WA_INACTIVE);
		}
		break;
		}

		return Original_MainWindowWndProcA(window, message, wParam, lParam);
	}

	static void OnPluginInitialize()
	{
		APLogger::print(__FUNCTION__"(): EnableDebug is initializing...\n");

		LocalPath = std::filesystem::current_path();
		configReload(LocalPath / "config.toml");

		Original_ChangeGameState = reinterpret_cast<void(__fastcall*)(GameState)>(memSigScan("\x48\x83\xEC\x28\xE8\x00\x00\x00\x00\x83\x78\x04\x05\x75\x06\x80\x78\x10\x00\x75\x0B\x89\x48\x04\xC6\x40\x10\x01\xC6\x40\x2C\x00\x48\x83\xC4\x28\xC3", "xxxxx????xxxxxxxxxxxxxxxxxxxxxxxxxxxx", (void*)0x1402c4bb0));
		Original_ChangeGameSubState = reinterpret_cast<void(__fastcall*)(GameState, GameSubState)>(memSigScan("\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x20\x89\xD6\xE8\x00\x00\x00\x00\x31\xDB\x48\x89\xC7\x83\xF9\x0C", "xxxx?xxxx?xxxxxxxx????xxxxxxxx", (void*)0x1527e49e0));

		Win32Hacks::HooksBegin(::GetModuleHandleW(nullptr));
		{
			Win32Hacks::WriteToProtectedMemory(reinterpret_cast<void*>(memSigScan("\x74\x21\x83\xF9\x08\x74\x11\xB9\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x32\xC0\x48\x83\xC4\x28\xC3\x8B\x0D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x32\xC0\x48\x83\xC4\x28\xC3", "xxxxxxxx????x????xxxxxxxxx????x????xxxxxxx", (void*)0x140441153)), 5, "\xE9\x1E\x00\x00\x00"); // NOTE: "jz" -> "jmp"
			Win32Hacks::HookFunction(reinterpret_cast<void*>(memSigScan("\x40\x53\x48\x83\xEC\x40\x48\x8D\x4C\x24\x00\xE8\x00\x00\x00\x00\x48\x8B\x08\x48\x89\x0D\x00\x00\x00\x00\x48\x8D\x0D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x48\x29\x05\x00\x00\x00\x00\x48\x8D\x0D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x33\xD2\x48\x8D\x4C\x24\x00\xE8\x00\x00\x00\x00\x48\x8D\x05\x00\x00\x00\x00\x48\x89\x44\x24\x00\xE8\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x48\x8B\x10\x48\x8B\xC8\xFF\x52\x18\x90\x48\x8D\x54\x24\x00\x48\x8D\x4C\x24\x00\xE8\x00\x00\x00\x00\x48\x8B\x08\x48\x89\x0D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x84\xC0\x74\x0D\xE8\x00\x00\x00\x00\x84\xC0\x74\x04\x32\xDB\xEB\x02", "xxxxxxxxxx?x????xxxxxx????xxx????x????xxx????xxx????x????x????xxxxxx?x????xxx????xxxx?x????x????x????x????xxxxxxxxxxxxxx?xxxx?x????xxxxxx????x????x????x????xxxxx????xxxxxxxx", (void*)0x1402b68c0)), Original_EngineUpdateTick, Hooked_EngineUpdateTick);
			Win32Hacks::HookFunction(reinterpret_cast<void*>(memSigScan("\x48\x89\x5C\x24\x00\x48\x89\x6C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x20\x49\x8B\xF1\x49\x8B\xF8\x8B\xDA\x48\x8B\xE9\x81\xFA\x00\x00\x00\x00", "xxxx?xxxx?xxxx?xxxxxxxxxx?x?xxxxx????", (void*)0x1402c2c50)), Original_MainWindowWndProcA, Hooked_MainWindowWndProcA);
		}
		Win32Hacks::HooksEnd();
	}
}

LRESULT WINAPI DLLWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	//THIS FUNCTION CAN LIKELY BE SCRAPPED LATER
	switch (msg)
	{
		// handle other messages.
	case WM_COMMAND:
		using namespace EnableDebugMode_AP;
		Original_ChangeGameSubState(static_cast<GameState>(0), static_cast<GameSubState>(1));
		CurrentState = PluginState::InDataTest;
		return 0;

	default: // anything we dont handle.
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return 0; // just in case
}

extern "C"
{
	__declspec(dllexport) void PreInit()
	{
		EnableDebugMode_AP::OnPluginInitialize();
	}
}
