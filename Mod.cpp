#include "APClient.h"
#include "APDeathLink.h"
#include "APGUI.h"
#include "APIDHandler.h"
#include "APReload.h"
#include "APTraps.h"
#include "Diva.h"
#include "pch.h"
#include <Archipelago.h>
#include <d3d11.h>
#include <detours.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

HOOK(bool, __fastcall, _InputEverythingElse, 0x1402AB070, long long a1, int btn)
{
    return ImGui::GetIO().WantCaptureKeyboard ? false : original_InputEverythingElse(a1, btn);
}

HOOK(bool, __fastcall, _InputAcceptBack, 0x1402AAF80, long long a1, int btn)
{
    return ImGui::GetIO().WantCaptureKeyboard ? false : original_InputAcceptBack(a1, btn);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    if (ImGui::GetIO().WantCaptureMouse)
    {
        switch (msg)
        {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MOUSEMOVE:
        case WM_MOUSEWHEEL:
            return 0;
        }
    }

    return CallWindowProc(APGUI::g_OriginalWndProc, hWnd, msg, wParam, lParam);
}

HOOK(void, __fastcall, _PvResultsFinalize, 0x14024B800, char* PvPlayData, long long a2)
{
    // This might be somewhere in PvPlayData without having to call out
    auto PvGameData = (char*)reinterpret_cast<uint64_t(__fastcall*)(void)>(0x14027DD90)();
    int diff[3];
    memcpy(diff, PvGameData, 3 * sizeof(int));

    // A grade of 1 happens only at playerPercent < 40% (good luck surviving above Easy)
    // Instead of AP patching the comparison, recheck it here.
    auto &pvID = *reinterpret_cast<int*>(PvPlayData + 0x10);
    auto &pvName = *reinterpret_cast<std::string*>(PvPlayData + 0x2CEF8);
    auto &playerGrade = *reinterpret_cast<int*>(PvPlayData + 0x2D190);
    auto &playerPercent = *reinterpret_cast<int*>(PvPlayData + 0x2D304);
    auto &clearPercent = *reinterpret_cast<int*>(PvPlayData + 0x2D308);

    if (playerGrade == 2 && playerPercent < clearPercent)
        playerGrade = 1; // "Cheap"

    APLogger::print("Finished ID %i with grade %i\n", pvID, playerGrade);

    if (playerGrade >= APClient::clearGrade) {
        APClient::LocationSend(pvID);
    }
    else {
        //playerGrade = 0; // Potentially use the UI to communicate clearGrade?
        APDeathLink::runAmnesty();
        APDeathLink::deathLinked = true;
    }

    // Run the original function, resets playerGrade to what it should be (if previously downgraded to Cheap)
    original_PvResultsFinalize(PvPlayData, a2);
}

HOOK(void, __fastcall, _PvLoop, 0x140244BA0, char* PvPlayData) {
    original_PvLoop(PvPlayData);

    APDeathLink::run(false);
    APTraps::run();
}

HOOK(void, __fastcall, _PvCalculateGrade, 0x1402462E0, char* PvPlayData) {
    // Too early for AP's UX but a better hook than before.
    // Primarily to catch the FAILURE on 0 HP (for AP's UX).

    APDeathLink::check_fail();
    APTraps::reset();

    original_PvCalculateGrade(PvPlayData);
}

HOOK(bool, __fastcall, _ModifierSudden, 0x14024b720, long long a1) {
    return APTraps::isSudden ? true : original_ModifierSudden(a1);
}

HOOK(bool, __fastcall, _ModifierHidden, 0x14024b730, long long a1) {
    return APTraps::isHidden ? true : original_ModifierHidden(a1);
}

HOOK(float, __fastcall, _SafetyDuration, 0x14024a5f0, long long a1) {
    auto time = original_SafetyDuration(a1);

    APDeathLink::safetyExpired = (time <= 0.0f);
    if (APDeathLink::safetyExpired && APDeathLink::HPnumerator < APDeathLink::HPdenominator)
        return 0.39f;

    return time;
}

HOOK(char**, __fastcall, _ReadDBLine, 0x1404C5950, uint64_t a1, char** pv_db_prop) {
    std::string line(pv_db_prop[0], pv_db_prop[1]);
    char** original = original_ReadDBLine(a1, pv_db_prop);

    if (original != nullptr && **original >= '1' && **original <= '2' && !APIDHandler::check(line))
        **original = '0';

    return original;
}

HOOK(void, __fastcall, _ChangeGameSubState, 0x1527E49E0, int state, int substate) {
    // This is most likely a greedy hook (especially against Debug without sigscanning).
    // If it becomes a problem, directly watching state change bytes is possible from 0x1402C4810()

    static bool skipped = false;

    if (state == 2 && substate == 47 || state == 12 && substate == 5) {
        APTraps::reset();
    }
    else if (state == 0 || state == 3) {
        skipped = false;
    }
    else if (state == 9 && substate == 47 || state == 6 && substate == 47) {
        bool reload_was_needed = APIDHandler::reload_needed;
        APIDHandler::reload_needed = false;
        APIDHandler::unlock();

        if (reload_was_needed) {
            if (APClient::recvIDs.size() > 0) {
                APLogger::print("Forcing needed reload (have IDs)\n");
                original_ChangeGameSubState(0, 1);
                return;
            }
            else {
                APLogger::print("Skipped needed reload (no IDs)\n");
            }
        }

       if (APReload::skipMainMenu && skipped == false) {
            APLogger::print("Skipping main menu (state: %d)\n", state);
            skipped = true;
            original_ChangeGameSubState(2, 47);
            return;
        }
    }

    original_ChangeGameSubState(state, substate);
}

HOOK(void, __fastcall, _cust_null, 0x1405946E0, long long* a1, unsigned int a2, char a3, long long a4) {
    // When entering Customize: Suppress an intermittent nullptr at 0x1405947A7 related to reloading and possibly modules.
    // It should not be handheld this way, but it's better than a game crash?

    if (a1 == nullptr)
        return;

    original_cust_null(a1, a2, a3, a4);
}

HOOK(void, __fastcall, _load_null, 0x1405948E0, long long* a1, unsigned long long a2, unsigned long long a3, unsigned long long a4) {
    // When entering Gameplay: Suppress an intermittent nullptr at 0x1405949D9 related to reloading and possibly modules.
    // It should not be handheld this way, but it's better than a game crash?

    // 3D PVs will have broken/invis modules.
    if (a1 == nullptr)
        return;

    original_load_null(a1, a2, a3, a4);
}

extern "C"
{
    void __declspec(dllexport) OnFrame(IDXGISwapChain* swapChain)
    {
        APClient::CheckMessages();

        APGUI::init(swapChain);
        if (!APGUI::g_OriginalWndProc)
            APGUI::g_OriginalWndProc = (WNDPROC)SetWindowLongPtr(APGUI::g_hWnd, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
        APGUI::onFrame();

        if (APGUI::g_ImGuiInitialized && !ImGui::GetIO().WantCaptureKeyboard)
            APReload::scan();
    }

    void __declspec(dllexport) Init()
    {
        // May cause an APCpp crash? Not required.
        //freopen("CONOUT$", "w", stdout);

        INSTALL_HOOK(_PvResultsFinalize);
        INSTALL_HOOK(_PvLoop);
        INSTALL_HOOK(_PvCalculateGrade);
        INSTALL_HOOK(_ModifierSudden);
        INSTALL_HOOK(_ModifierHidden);
        INSTALL_HOOK(_SafetyDuration);

        INSTALL_HOOK(_ChangeGameSubState);
        INSTALL_HOOK(_ReadDBLine);
        INSTALL_HOOK(_load_null);
        INSTALL_HOOK(_cust_null);

        INSTALL_HOOK(_InputAcceptBack);
        INSTALL_HOOK(_InputEverythingElse);
    }
}
