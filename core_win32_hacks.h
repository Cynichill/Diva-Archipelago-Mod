#pragma once
#include "core_types.h"

#include <Windows.h>
#include <detours.h>

#include <vector>

namespace Win32Hacks
{
	template <typename Func>
	inline void MakeMemoryWritable(void* address, size_t byteSize, Func onWritable)
	{
		DWORD oldProtect {}, newProtect {};
		if (::VirtualProtect(reinterpret_cast<LPVOID>(address), static_cast<SIZE_T>(byteSize), PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			onWritable();
			::VirtualProtect(reinterpret_cast<LPVOID>(address), static_cast<SIZE_T>(byteSize), oldProtect, &newProtect);
		}
	}

	inline std::vector<uint8_t> Backup(void* address, size_t byteSize)
	{
		std::vector<uint8_t> result;
		for (uint64_t i = 0; i < byteSize; i++)
		{
			uint8_t bk = *(uint8_t*)((uint64_t)address + i);
			result.push_back(bk);
		}
		return result;
	}

	inline void WriteToProtectedMemory(void* address, size_t dataSize, const void* dataToCopy)
	{
		MakeMemoryWritable(address, dataSize, [&] { ::memcpy(reinterpret_cast<void*>(address), dataToCopy, dataSize); });
	}

	inline void PatchNopInstructions(void* address, size_t dataSize)
	{
		MakeMemoryWritable(address, dataSize, [&] { ::memset(reinterpret_cast<void*>(address), 0x90909090, dataSize); });
	}

	inline void HooksBegin(HMODULE moduleHandle)
	{
		::DisableThreadLibraryCalls(moduleHandle);
		::DetourTransactionBegin();
		::DetourUpdateThread(::GetCurrentThread());
	}

	inline void HooksEnd()
	{
		::DetourTransactionCommit();
	}

	template <typename Func>
	inline void HookFunction(void* address, Func*& outOriginalFunc, Func& onHookFunc)
	{
		static_assert(std::is_function_v<Func>);

		outOriginalFunc = reinterpret_cast<Func*>(address);
		::DetourAttach(reinterpret_cast<PVOID*>(&outOriginalFunc), &onHookFunc);
	}

	template <typename Func>
	inline void HookFunctionPointer(void* address, Func*& outOriginalFunc, Func& onHookFunc)
	{
		static_assert(std::is_function_v<Func>);
		// TODO: This should probably dereference the function pointer and then insert a proper hook at that address instead
		MakeMemoryWritable(address, sizeof(Func**), [&] { outOriginalFunc = *reinterpret_cast<Func**>(address); *reinterpret_cast<Func**>(address) = &onHookFunc; });
	}

	inline void ClearConsole()
	{
		::system("cls");
	}

	inline void BringConsoleToForeground()
	{
		if (const HWND consoleHandle = ::GetConsoleWindow(); consoleHandle != NULL)
		{
			::ShowWindow(consoleHandle, SW_SHOW);
			WINDOWPLACEMENT placement = { sizeof(WINDOWPLACEMENT) };
			::GetWindowPlacement(consoleHandle, &placement);
			switch (placement.showCmd)
			{
			case SW_SHOWMAXIMIZED: ::ShowWindow(consoleHandle, SW_SHOWMAXIMIZED); break;
			case SW_SHOWMINIMIZED: ::ShowWindow(consoleHandle, SW_RESTORE); break;
			default: ::ShowWindow(consoleHandle, SW_NORMAL); break;
			}
			::SetWindowPos(0, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE);
			::SetForegroundWindow(consoleHandle);
		}
	}

}
