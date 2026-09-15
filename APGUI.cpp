#include "APClient.h"
#include "APDeathLink.h"
#include "APGUI.h"
#include "APHints.h"
#include "APIDHandler.h"
#include "APLogger.h"
#include "APReload.h"
#include "APSettings.h"
#include "APTraps.h"

namespace APGUI
{
    // Configurables

    bool& devMode = APClient::devMode;
    bool enableDocking = false;
    bool autoHideClient = true; // Hide Client during gameplay
    bool showWarning = true; // First run warning
    float alphaDefault = 1.0f;
    float alphaIngame = 1.0f;

    bool inlineTooltips = true; // True: Help tooltips become regular hovers instead of (?)

    bool showImGuiDemo = false;
    bool firstFrame = true;

    // TODO: Move names into own namespace?
    std::vector<std::pair<const char*, std::function<void()>>> windows = {
        { "Tracker", APIDHandler::ImGuiTab },
        { "Hints", APHints::ImGuiTab },
        { "Death Link", APDeathLink::ImGuiTab },
        { "Traps", APTraps::ImGuiTab },
    };

    ID3D11Device* g_Device = nullptr;
    ID3D11DeviceContext* g_Context = nullptr;
    HWND g_hWnd = nullptr;
    WNDPROC g_OriginalWndProc = nullptr;

    void init(IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11DeviceContext* deviceContext)
    {
        ImGui_ImplWin32_EnableDpiAwareness();
        float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

        g_Device = device;
        g_Context = deviceContext;

        // Get window handle
        DXGI_SWAP_CHAIN_DESC desc;
        swapChain->GetDesc(&desc);
        g_hWnd = desc.OutputWindow;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;

        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(main_scale);
        style.FontScaleDpi = main_scale;

        ImGui_ImplWin32_Init(g_hWnd);
        ImGui_ImplDX11_Init(g_Device, g_Context);

        APSettings::load();
    }

    bool isInGame()
    {
        return *(bool*)PvPlayData && !*(bool*)(PvPlayData + 0x1) && !*(bool*)(PvPlayData + 0x2D17D);
    }

    void onFrame(IDXGISwapChain* swapChain)
    {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Weird focus behavior on create so keep every frame.
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingOverCentralNode);

        if (isInGame() && autoHideClient) {
            ImGui::SetWindowFocus(nullptr);
            ImGui::GetIO().WantCaptureKeyboard = false;
            ImGui::GetIO().WantCaptureMouse = false;
            ImGui::SetNextFrameWantCaptureKeyboard(false);
            ImGui::SetNextFrameWantCaptureMouse(false);

            ImGui::Render();
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            return;
        }
        while (ShowCursor(true) < 1); // If the GUI is visible, the cursor should be too.
        ImGui::GetStyle().Alpha = isInGame() ? alphaIngame : alphaDefault;

        if (showImGuiDemo)
            ImGui::ShowDemoWindow();

        ImGuiID client_dockspace_id = ImGui::GetID("client");
        //ImGui::DockSpace(client_dockspace_id);

        if (enableDocking) {
            ImGui::SetNextWindowDockID(client_dockspace_id, ImGuiCond_FirstUseEver);
            ImGui::Begin("Client", nullptr, ImGuiWindowFlags_NoFocusOnAppearing);
            APClient::ImGuiTab();
            ImGui::End();
        }
        else {
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoFocusOnAppearing;
            //if (firstFrame)
            //flags |= ImGuiWindowFlags_AlwaysAutoResize;
            ImGui::SetNextWindowSizeConstraints(ImVec2(360, 180), ImVec2(FLT_MAX, FLT_MAX));
            ImGui::Begin("Archipelago Mod", nullptr, flags);

            ImGui::BeginTabBar("##clientTabs");
            if (ImGui::BeginTabItem("Client", nullptr/*, ImGuiTabItemFlags_SetSelected */)) {
                APClient::ImGuiTab();
                ImGui::EndTabItem();
            }
        }

        if (devMode || AP_GetConnectionStatus() == AP_ConnectionStatus::Authenticated) {
            for (const auto& [name, contents] : windows) {
                if (enableDocking) {
                    ImGui::SetNextWindowDockID(client_dockspace_id, ImGuiCond_FirstUseEver);
                    ImGui::Begin(name, nullptr, ImGuiWindowFlags_NoFocusOnAppearing);
                    contents();
                    ImGui::End();
                }
                else {
                    if (ImGui::BeginTabItem(name)) {
                        contents();
                        ImGui::EndTabItem();
                    }
                }
            }
        }

        if (firstFrame) {
            firstFrame = false;
            ImGui::SetWindowFocus("Client");
            ImGui::SetWindowFocus(0);
        }
        else {
            ImVec2 display_size = ImGui::GetIO().DisplaySize;
            ImVec2 window_pos = ImVec2(display_size.x - ImGui::GetWindowWidth() - (display_size.x * static_cast<float>(0.01)),
                display_size.y - ImGui::GetWindowHeight() - (display_size.y * static_cast<float>(0.01)));

            ImGui::SetWindowPos(window_pos, ImGuiCond_FirstUseEver);
        }

        if (enableDocking) {
            ImGui::SetNextWindowDockID(client_dockspace_id, ImGuiCond_FirstUseEver);
            ImGui::Begin("Advanced", nullptr, ImGuiWindowFlags_NoFocusOnAppearing);
            APGUI::ImGuiTab();
            ImGui::End();
        }
        else {
            if (ImGui::BeginTabItem("Advanced")) {
                APGUI::ImGuiTab();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
            ImGui::End();
        }

        warning();

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void config(const toml::table& settings)
    {
        toml::table section;
        if (settings.contains("gui") && settings["gui"].is_table())
            section = *settings["gui"].as_table();

        autoHideClient = section["autoHideClient"].value_or(true);
        showWarning = section["warning"].value_or(true);
        enableDocking = section["docking"].value_or(false);

        float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));
        auto scale = section["fontScale"].value_or(main_scale);
        scale = std::clamp(scale, 0.75f, 4.0f);
        ImGui::GetStyle().FontScaleDpi = scale;

        float _alphaDefault = section["alphaDefault"].value_or(1.0f);
        alphaDefault = std::clamp(_alphaDefault, 0.5f, 1.0f);

        float _alphaIngame = section["alphaIngame"].value_or(1.0f);
        alphaIngame = std::clamp(_alphaIngame, 0.1f, 1.0f);
    }

    void save(toml::table &settings)
    {
        toml::table config;
        config.insert("autoHideClient", autoHideClient);
        config.insert("docking", enableDocking);
        config.insert("fontScale", ImGui::GetStyle().FontScaleDpi);
        config.insert("warning", showWarning);
        config.insert("alphaDefault", alphaDefault);
        config.insert("alphaIngame", alphaIngame);

        settings.insert("gui", config);
    }

    void warning()
    {
        if (!showWarning)
            return;

        ImGui::OpenPopup("Archipelago Mod - First Run");
        if (ImGui::BeginPopupModal("Archipelago Mod - First Run", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize))
        {
            ImGui::SetWindowFocus("Archipelago Mod - First Run");

            ImGui::Text("This mod is for use with");
            ImGui::SameLine();
            ImGui::TextLinkOpenURL("Archipelago", "https://archipelago.gg");
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::Text(", a multi-game randomizer.");
            ImGui::Text("\nFor more information, check the Help section under the Advanced tab.");

            ImGui::Separator();
            if (ImGui::Button("Okay"))
            {
                showWarning = false;
                APSettings::save();

                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            ImGui::EndPopup();
        }
    }

    void ImGuiTab()
    {
        APSettings::ImGuiTab();

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Help")) {
            /*if (ImGui::Button("Show first run warning")) {
                showWarning = true;
                warning();
            }*/
            ImGui::TextLinkOpenURL("Archipelago website", "https://archipelago.gg");
            ImGui::TextLinkOpenURL("Project Diva AP documentation", "https://github.com/Cynichill/DivaAPworld/tree/main/docs");
            ImGui::TextLinkOpenURL("Project Diva AP Discord thread", "https://discord.com/channels/731205301247803413/1241134454391443580");
        }

        APReload::ImGuiTab();

        if (ImGui::CollapsingHeader("Styling")) {
            ImGui::Checkbox("Hide during gameplay", &autoHideClient);
            ImGui::Checkbox("Enable docking support", &enableDocking);
            HelpMarker("Instead of a single window with tabs, spawn each tab as its own window for more customization.");

            ImGui::Checkbox("Show ImGui demo", &showImGuiDemo);
            ImGui::Checkbox("Inline help tooltips", &inlineTooltips);
            HelpMarker("That's me!");
            ImGui::DragFloat("Font DPI Scale", &ImGui::GetStyle().FontScaleDpi, 0.02f, 0.75f, 4.0f, "%.02f", ImGuiSliderFlags_AlwaysClamp);
            HelpMarker("1.25 recommended for 1440p\n1.75 recommended for 4K");

            ImGui::DragFloat("Default Alpha", &alphaDefault, 0.01f, 0.5f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat("In-game Alpha", &alphaIngame, 0.01f, 0.1f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            HelpMarker("If not hidden during gameplay, lower alpha to this instead.");
        }

        if (ImGui::CollapsingHeader("Developer Mode")) {
            ImGui::Checkbox("Enable Developer Mode", &devMode);
            HelpMarker("Dangerous! For the curious or the stuck.");

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

                HelpMarker("Fills the IDHandler with \"random\" IDs up to 10000.\n"
                    "Try toggling Freeplay from the Tracker tab.\n"
                    "Effectively an offline Archipelago."
                );

                ImGui::SameLine();
                ImGui::Text("%d/%d recv/seed", APClient::recvIDs.size(), APClient::seedIDs.size());

                ImGui::Text("AP_ID_FACTOR: %i", AP_ID_FACTOR);

                APLogger::ImGuiTab();
            }
        }
    }
}