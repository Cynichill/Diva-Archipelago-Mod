#include "pch.h"
#include "APClient.h"
#include "APDeathLink.h"
#include "APGUI.h"
#include "APIDHandler.h"
#include "APReload.h"
#include "APTraps.h"
#include "Diva.h"
#include "SigScan.h"

// 0x1402AB070
void* InputEverythingElse = sigScan("\x4c\x63\xc2\x49\x81\xf8\xa2\x00\x00\x00\x73\x1d\x41\x0f\xb6\xc0\x49\xc1\xe8\x06\x24\x3f\x0f\xb6\xd0\x4a\x8b\x84\xc1\x90\x00\x00\x00", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
HOOK(bool, __fastcall, _InputEverythingElse, InputEverythingElse, long long a1, int btn)
{
    return ImGui::GetIO().WantCaptureKeyboard ? false : original_InputEverythingElse(a1, btn);
}

// 0x1402AAF80
void* InputAcceptBack = sigScan("\x4c\x63\xc2\x49\x81\xf8\xa2\x00\x00\x00\x73\x19", "xxxxxxxxxxxx");
HOOK(bool, __fastcall, _InputAcceptBack, InputAcceptBack, long long a1, int btn)
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

// 0x14024B800
void* PvResultsFinalize = sigScan("\x48\x89\x5c\x24\x10\x48\x89\x74\x24\x18\x55\x57\x41\x54\x41\x56\x41\x57\x48\x8d\xac\x24\x90\xfe\xff\xff", "xxxxxxxxxxxxxxxxxxxxxxxxxx");
HOOK(void, __fastcall, _PvResultsFinalize, PvResultsFinalize, char* PvPlayData, long long a2)
{
    if (!APClient::devMode && AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated) {
        original_PvResultsFinalize(PvPlayData, a2);
        return;
    }

    // This might be somewhere in PvPlayData without having to call out
    // TODO: Client-side per-diff clear grades?

    //auto PvGameData = (char*)reinterpret_cast<uint64_t(__fastcall*)(void)>(0x14027DD90)();
    //int diff[3];
    //memcpy(diff, PvGameData, 3 * sizeof(int));

    // A grade of 1 happens only at playerPercent < 40% (good luck surviving above Easy)
    // Instead of AP patching the comparison, recheck it here.
    auto &pvID = *reinterpret_cast<int*>(PvPlayData + 0x10);
    auto &pvName = *reinterpret_cast<std::string*>(PvPlayData + 0x2CEF8);
    // Pull playerGrade out for now instead of referencing. Potentially use the UI to communicate clearGrade?
    int playerGrade = *reinterpret_cast<int*>(PvPlayData + 0x2D190);
    auto &playerPercent = *reinterpret_cast<int*>(PvPlayData + 0x2D304);
    auto &clearPercent = *reinterpret_cast<int*>(PvPlayData + 0x2D308);

    if (playerGrade == 2 && playerPercent < clearPercent)
        playerGrade = 1; // "Cheap"

    APLogger::print("Finished ID %i with grade %i >= %i\n", pvID, playerGrade, APClient::clearGrade);

    if (playerGrade >= APClient::clearGrade) {
        APClient::LocationSend(pvID);
    }
    else {
        APDeathLink::runAmnesty();
        APDeathLink::deathLinked = true;
    }

    original_PvResultsFinalize(PvPlayData, a2);
}


// 0x140244BA0
void* PvLoop = sigScan("\x48\x89\x5c\x24\x10\x48\x89\x74\x24\x18\x57\x48\x83\xec\x20\x48\x8b\xf9\x33\xdb\xe8\xe7\x91\x03\x00", "xxxxxxxxxxxxxxxxxxxxxxxxx");
HOOK(void, __fastcall, _PvLoop, PvLoop, char* PvPlayData) {
    if (APClient::devMode || AP_GetConnectionStatus() == AP_ConnectionStatus::Authenticated) {
        APDeathLink::run(false);
        APTraps::run();
    }

    original_PvLoop(PvPlayData);
}

// 0x1402462E0
void* PvCalculateGrade = sigScan("\x48\x83\xec\x28\x80\xb9\xad\xd3\x02\x00\x00\x0f\x84\xc1\x00\x00\x00\xf3\x0f\x10\x81\x04\xd3\x02\x00\xe8\x92\x7a\x03\x00", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
HOOK(void, __fastcall, _PvCalculateGrade, 0x1402462E0, char* PvPlayData) {
    // Too early for AP's UX but a better hook than before.
    // Primarily to catch the FAILURE on 0 HP (for AP's UX).
    if (APClient::devMode || AP_GetConnectionStatus() == AP_ConnectionStatus::Authenticated) {
        APDeathLink::check_fail();
        APTraps::reset();
    }

    original_PvCalculateGrade(PvPlayData);
}

// 0x14FB926F0
HOOK(void, __fastcall, _PvUpdateHP, 0x14fb926f0, uintptr_t PvPlayData, int a2, char a3, char a4, uint64_t a5, char a6, char a7, char a8) {
    // That's a lot of parameters.
    // If HP is 0 after plus other criteria, can auto-retry the song.

    original_PvUpdateHP(PvPlayData, a2, a3, a4, a5, a6, a7, a8);

    if (!APClient::devMode && AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated)
        return;

    int& HP = *(int*)(PvPlayData + 0x2D234);
    bool& noFail = *(bool*)(PvPlayData + 0x2D31D);
    const bool& maxCombo = *(int*)(PvPlayData + 0x2D25C) > 0;
    if (HP == 0 && !noFail) {
        APTraps::reset();
        APDeathLink::check_fail();

        if (APDeathLink::auto_retry && maxCombo)
            APDeathLink::resetSong();
        //else APDeathLink::deathLinked = true;
    }
}

// 0x14024B720
void* ModifierSudden = sigScan("\x83\xb9\x20\xd1\x02\x00\x03\x0f\x94\xc0\xc3", "xxxxxxxxxxx");
HOOK(bool, __fastcall, _ModifierSudden, ModifierSudden, long long a1) {
    return APTraps::isSudden || original_ModifierSudden(a1);
}

// 0x14024B730
void* ModifierHidden = sigScan("\x83\xb9\x20\xd1\x02\x00\x02\x0f\x94\xc0\xc3", "xxxxxxxxxxx");
HOOK(bool, __fastcall, _ModifierHidden, ModifierHidden, long long a1) {
    return APTraps::isHidden || original_ModifierHidden(a1);
}

// 0x14024A5F0
void* SafetyDuration = sigScan("\x66\x0f\x6e\x81\x10\xd3\x02\x00\x0f\x57\xc9\x0f\x5b\xc0\xf3\x0f\x5c\x81\x3c\xd3\x02\x00\xf3\x0f\x5f\xc1\xc3", "xxxxxxxxxxxxxxxxxxxxxxxxxxx");
HOOK(float, __fastcall, _SafetyDuration, SafetyDuration, long long a1) {
    auto time = original_SafetyDuration(a1);

    APDeathLink::safetyExpired = (time <= 0.0f);
    if (APDeathLink::safetyExpired && APDeathLink::HPnumerator < APDeathLink::HPdenominator)
        return 0.39f;

    return time;
}

// 0x1404C5950
void* ReadDBLine = sigScan("\x48\x83\xec\x38\x80\x39\x00\x48\x8b\x02\x4c\x8b\x42\x08\x48\x8d\x54\x24\x20\x48\x89\x44\x24\x20\x4c\x89\x44\x24\x28\x74\x12", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
HOOK(char**, __fastcall, _ReadDBLine, ReadDBLine, uint64_t a1, char** pv_db_prop) {
    std::string line(pv_db_prop[0], pv_db_prop[1]);
    char** original = original_ReadDBLine(a1, pv_db_prop);

    if (original != nullptr && **original >= '1' && **original <= '2' && !APIDHandler::check(line))
        **original = '0';

    return original;
}

// 0x1527E49E0
void* ChangeGameSubState = sigScan("\x48\x89\x5c\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xec\x20\x89\xd6\xe8\x1a\xfe\xad\xed", "xxxxxxxxxxxxxxxxxxxxxx");
HOOK(void, __fastcall, _ChangeGameSubState, ChangeGameSubState, int state, int substate) {
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
        APIDHandler::unlock();

       if (APReload::skipMainMenu && skipped == false) {
            APLogger::print("Skipping main menu (state: %d)\n", state);
            skipped = true;
            original_ChangeGameSubState(2, 47);
            return;
        }
    }

    original_ChangeGameSubState(state, substate);
}

// 0x1405946E0
void* cust_null = sigScan("\x40\x53\x55\x57\x48\x83\xec\x70\x48\x8b\x05\x79\x7c\x80\x00", "xxxxxxxxxxxxxxx");
HOOK(void, __fastcall, _cust_null, cust_null, long long* a1, unsigned int a2, char a3, long long a4) {
    // When entering Customize: Suppress an intermittent nullptr at 0x1405947A7 related to reloading and possibly modules.
    // It should not be handheld this way, but it's better than a game crash?

    if (a1 == nullptr)
        return;

    original_cust_null(a1, a2, a3, a4);
}

// 0x1405948E0
void* load_null = sigScan("\x40\x57\x48\x81\xec\xd0\x00\x00\x00\x48\x8b\x05\x78\x7a\x80\x00\x48\x33\xc4", "xxxxxxxxxxxxxxxxxxx");
HOOK(void, __fastcall, _load_null, load_null, long long* a1, unsigned long long a2, unsigned long long a3, unsigned long long a4) {
    // When entering Gameplay: Suppress an intermittent nullptr at 0x1405949D9 related to reloading and possibly modules.
    // It should not be handheld this way, but it's better than a game crash?

    // 3D PVs will have broken/invis modules.
    if (a1 == nullptr)
        return;

    original_load_null(a1, a2, a3, a4);
}

// 0x14027BB00
void* PvGameApplyDiff = sigScan("\x48\x8b\x01\x89\x50\x08\xc3", "xxxxxxx");
HOOK(void, __fastcall, _PvGameApplyDiff, PvGameApplyDiff, long long* data, int diff)
{
    // Dodging hooks from at least X SP and New Classics.
    // Allow ID 700 (Ievan Polkka Tutorial) to be things other than Easy.
    // TODO: Not this. Find out where the force to Easy happens: 0x15E4BD270()

    if (APClient::devMode || AP_GetConnectionStatus() == AP_ConnectionStatus::Authenticated) {
        const auto& pvID = *reinterpret_cast<int*>((char*)*data + 0x4);

        if (pvID == 700) {
            // Use last played base difficulty (ExEx might require shipping it in mod_pv_db, so skip for now)
            auto PvGameData = (char*)reinterpret_cast<uint64_t(__fastcall*)(void)>(0x14027DD90)();
            diff = *reinterpret_cast<int*>((char*)PvGameData + 0x4);
            APLogger::print("Overriding ID 700 diff to %i\n", diff);
        }
    }

    original_PvGameApplyDiff(data, diff);
}

extern "C"
{
    void __declspec(dllexport) D3DInit(IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11DeviceContext* deviceContext)
    {
        APGUI::init(swapChain, device, deviceContext);
        APGUI::g_OriginalWndProc = (WNDPROC)SetWindowLongPtr(APGUI::g_hWnd, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
    }

    void __declspec(dllexport) OnFrame(IDXGISwapChain* swapChain)
    {
        APClient::CheckMessages();
        APGUI::onFrame(swapChain);
        APIDHandler::slowReleaseRun();

        if (APClient::devMode || AP_GetConnectionStatus() == AP_ConnectionStatus::Authenticated) {
            APTraps::runFrame();
        }

        if (!ImGui::GetIO().WantCaptureKeyboard)
            APReload::scan();
    }

    void __declspec(dllexport) Init()
    {
        // May cause an APCpp crash? Not required.
        //freopen("CONOUT$", "w", stdout);
        AP_SetLoggingCallback(APLogger::fromAPCpp);

        INSTALL_HOOK(_PvResultsFinalize);
        INSTALL_HOOK(_PvCalculateGrade);
        INSTALL_HOOK(_PvGameApplyDiff);
        INSTALL_HOOK(_PvLoop);
        INSTALL_HOOK(_PvUpdateHP);
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
