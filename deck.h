#pragma once
#include "core_win32_hacks.h"
extern LRESULT WINAPI DLLWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

class deck
{
public:
	struct stateStruct
	{
		uint16_t gameState;
		uint16_t gameSubState;
	};
};

namespace GameStates
{
	enum class GameState : i32
	{
		DATA_TEST = 3,
	};

	enum class GameSubState : i32
	{
		DATA_INITIALIZE = 0,
	};

}