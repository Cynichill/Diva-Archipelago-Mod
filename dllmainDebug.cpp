#include "MemSigScan.h"
#include "deck.h"
#include <chrono>
#include <thread>
#include "pch.h"

using namespace GameStates;

namespace EnableDebugMode
{
	enum class PluginState { RegularGame, WaitingToSelectDataTest, InDataTest };

	static PluginState CurrentState = PluginState::RegularGame;
	static bool IsMainWindowFocused = false;

	static auto Original_ChangeGameState = reinterpret_cast<void(__fastcall*)(GameState)>(0x00000001402C46F0);
	static auto Original_ChangeGameSubState = reinterpret_cast<void(__fastcall*)(GameState, GameSubState)>(0x000000015241DA80);

	static std::chrono::steady_clock::time_point conditionMetTimePoint;

	static bool IsKeyPressed(u8 keyCode) { return (::GetAsyncKeyState(keyCode) & 0x8000) != 0; }

	static void pluginLoop()
	{
		if (IsMainWindowFocused && IsKeyPressed(VK_F7))
		{
			//SWAP THIS LATER WITH DIRECT BUTTON CODE
			Original_ChangeGameState(GameState::DATA_TEST);
			CurrentState = PluginState::WaitingToSelectDataTest;

			// Create a thread to wait for 1 seconds before calling DLLWindowProc
			std::thread([]() {
				std::this_thread::sleep_for(std::chrono::seconds(1));
				DLLWindowProc(nullptr, WM_COMMAND, 0, 0);
				}).detach();
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
		printf(__FUNCTION__"(): EnableDebug is initializing...\n");

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
		using namespace EnableDebugMode;
		Original_ChangeGameSubState(static_cast<GameState>(0), static_cast<GameSubState>(0));
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
		EnableDebugMode::OnPluginInitialize();
	}
}
