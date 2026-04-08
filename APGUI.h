#pragma once
#include "pch.h"
#include <d3d11.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

namespace APGUI
{
    extern bool g_ImGuiInitialized;

    extern HWND g_hWnd;
    extern WNDPROC g_OriginalWndProc;

    void init(IDXGISwapChain* swapChain);
    void onFrame();
    void warning();
    void ImGuiTab();
}
