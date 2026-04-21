#include "APClient.h"
#include "APDeathLink.h"
#include "APGUI.h"
#include "APHints.h"
#include "APIDHandler.h"
#include "APLogger.h"
#include "APReload.h"
#include "APTraps.h"

namespace APGUI
{
    // Configurables

    bool auto_hide_client = true; // Hide Client during gameplay
    int& reloadDelay = APReload::reloadDelay;

    bool showImGuiDemo = false;
    bool &devMode = APClient::devMode;

    bool g_ImGuiInitialized = false;
    bool firstFrame = true;
    bool prevUnfocused = false;

    // First run warning
    namespace fs = std::filesystem;
    auto LocalPath = fs::current_path();
    fs::path ConfigTOML = LocalPath / "config.toml";
    fs::path reload_file = LocalPath / ".reload_warning";
    bool showWarning = true;

    ID3D11Device* g_Device = nullptr;
    ID3D11DeviceContext* g_Context = nullptr;
    HWND g_hWnd = nullptr;
    WNDPROC g_OriginalWndProc = nullptr;

    void init(IDXGISwapChain* swapChain)
    {
        if (g_ImGuiInitialized)
            return;

        ImGui_ImplWin32_EnableDpiAwareness();
        float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

        // Get device + context
        swapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_Device);
        g_Device->GetImmediateContext(&g_Context);

        // Get window handle
        DXGI_SWAP_CHAIN_DESC desc;
        swapChain->GetDesc(&desc);
        g_hWnd = desc.OutputWindow;

        // Init ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(main_scale);
        style.FontScaleDpi = main_scale;

        ImGui_ImplWin32_Init(g_hWnd);
        ImGui_ImplDX11_Init(g_Device, g_Context);

        g_ImGuiInitialized = true;
    }

    void onFrame()
    {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // auto hide client when in game, not paused, not on results
        auto PvPlayData = 0x1412C2330;
        if (auto_hide_client && *(bool*)PvPlayData && !*(bool*)(PvPlayData + 0x1) && !*(bool*)(PvPlayData + 0x2D17D)) {
            ImGui::GetIO().WantCaptureKeyboard = false;
            ImGui::GetIO().WantCaptureMouse = false;
            ImGui::SetNextFrameWantCaptureKeyboard(false);
            ImGui::SetNextFrameWantCaptureMouse(false);

            ImGui::Render();
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            return;
        }

        if (showImGuiDemo)
            ImGui::ShowDemoWindow();

        ImGui::Begin("Archipelago Mod###APClient", NULL,
                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);

        if (ImGui::BeginTabBar("APTabs" /*, ImGuiTabBarFlags_Reorderable*/)) {
            APClient::ImGuiTab();

            if (devMode || AP_GetConnectionStatus() == AP_ConnectionStatus::Authenticated) {
                APIDHandler::ImGuiTab();
                APHints::ImGuiTab();
                APDeathLink::ImGuiTab();
                APTraps::ImGuiTab();
            }

            //APLogger::ImGuiTab();

            APGUI::ImGuiTab();

            ImGui::EndTabBar();
        }

        if (!firstFrame)
        {
            ImVec2 display_size = ImGui::GetIO().DisplaySize;
            ImVec2 window_pos = ImVec2(display_size.x - ImGui::GetWindowWidth() - (display_size.x * static_cast<float>(0.01)),
                                       display_size.y - ImGui::GetWindowHeight() - (display_size.y * static_cast<float>(0.01)));

            ImGui::SetWindowPos(window_pos, ImGuiCond_FirstUseEver);
        }
        else {
            firstFrame = false;
            ImGui::SetWindowFocus(0);
        }

        warning();

        ImGui::End();

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void warning()
    {
        if (!showWarning || fs::exists(reload_file))
            return;

        try {
            // This is a wasteful read, but it "should" only ever happen one time ever
            std::ifstream file(ConfigTOML);
            auto data = toml::parse(file);

            ImGui::OpenPopup("Archipelago Mod - First Run");
            if (ImGui::BeginPopupModal("Archipelago Mod - First Run", NULL, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize))
            {
                ImGui::SetWindowFocus("Archipelago Mod - First Run");

                std::string warn1 = "After connecting, press the reload key while on the song list to get new songs.\n"
                    "Songs can be cleared on any available difficulty for the same checks.\n\n";
                ImGui::Text("%s", warn1.c_str());

                ImGui::Text("Defaults for some options can be configured in the mod's");
                ImGui::SameLine();
                ImGui::TextLinkOpenURL("config.toml", ConfigTOML.string().c_str());

                ImGui::Text("\nCurrent reload key: %s", (std::string)data["reload_key"].value_or("F7"));

                ImGui::Separator();
                if (ImGui::Button("I don't remember installing this mod"))
                {
                    showWarning = false;
                    std::ofstream reload_out(reload_file);
                    reload_out.close();
                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
                ImGui::EndPopup();
            }
        }
        catch (const std::exception& e) {
            APLogger::print("Error parsing config file: %s\n", e.what());
        }
    }

    void ImGuiTab()
    {
        if (ImGui::BeginTabItem("Advanced")) {
            ImGui::Checkbox("Hide Client during gameplay", &auto_hide_client);

            /*if (ImGui::Button("Reload config.toml"))
            ImGui::SameLine();*/
            ImGui::TextLinkOpenURL("Open config.toml", ConfigTOML.string().c_str());

            ImGui::Separator();

            if (ImGui::Button("Reload game"))
                APReload::run();

            ImGui::SameLine();
            if (ImGui::Button("Force"))
                APReload::ChangeGameState(1);

            ImGui::SameLine();
            ImGui::Text("Reload key: %s", APReload::reloadVal.c_str());
            ImGui::SliderInt("Reload delay", &reloadDelay, 1, 10);

            ImGui::Separator();

            ImGui::Checkbox("Show ImGui demo", &showImGuiDemo);
            ImGui::DragFloat("Font DPI Scale", &ImGui::GetStyle().FontScaleDpi, 0.02f, 0.75f, 4.0f, "%.02f");
            ImGui::SameLine();
            HelpMarker("1.25 recommended for 1440p\n1.75 recommended for 4K");

            if (ImGui::DragFloat("Global Alpha", &ImGui::GetStyle().Alpha, 0.01f, 0.50f, 1.0f, "%.2f"))
                ImGui::GetStyle().Alpha = max(ImGui::GetStyle().Alpha, 0.5f); // unlike the demo, actually prevent a 0

            ImGui::Separator();

            if (ImGui::Checkbox("AP Developer Mode", &devMode))
                devMode = false;
            if (ImGui::BeginPopupContextItem("##xx")){
                if (ImGui::MenuItem("Are you sure?##xx"))
                    devMode = !devMode;

                ImGui::EndPopup();
            }

            if (devMode) {
                // Easy crashes with other mods that already freopen'd to stdout
                /*if (!GetConsoleWindow() && ImGui::Button("Console")) {
                    AllocConsole();
                    APLogger::print("DO NOT CLOSE THIS WINDOW OR THE GAME WILL CLOSE\n");
                }*/

                if (ImGui::Button("Reset")) {
                    APClient::seedIDs.clear();
                    APClient::recvIDs.clear();
                    APClient::missingIDs.clear();

                    APReload::run();
                }

                ImGui::SameLine();
                if (ImGui::Button("Sample Random IDs")) {
                    APClient::seedIDs.clear();
                    APClient::seedIDs.push_back(0); // Prevent seedIDs == recvIDs
                    APClient::recvIDs.clear();

                    // This doesn't need good random. The biggest issue it will have is picking a valid ID.
                    for (int i = 0; i < 500; ++i)
                    {
                        int id = rand() % 10000 + 1;
                        APClient::seedIDs.push_back(id);

                        if (rand() % (rand() % 10 + 1) == 1)
                            APClient::PushRecvID(id);
                    }

                    APReload::run();
                }

                ImGui::SameLine();
                HelpMarker("Fills the IDHandler with \"random\" IDs up to 10000.\n"
                            "Try toggling Freeplay from the Tracker tab.");

                ImGui::SameLine();
                ImGui::Text("%d/%d recv/seed", APClient::recvIDs.size(), APClient::seedIDs.size());

                ImGui::Checkbox("Log to file", &APLogger::logToFile);
                if (&APLogger::logToFile) {
                    ImGui::SameLine();
                    ImGui::TextLinkOpenURL("Open log file", APLogger::LogPath.string().c_str());
                }

            }

            ImGui::EndTabItem();
        }
    }
}