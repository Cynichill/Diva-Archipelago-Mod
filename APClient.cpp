#include "APClient.h"
#include "APDeathLink.h"
#include "APGUI.h"
#include "APHints.h"
#include "APIDHandler.h"
#include "APReload.h"
#include "APTraps.h"

namespace APClient
{
    const char* GameName = "Hatsune Miku Project Diva Mega Mix+";

    bool devMode = false;

    // Any char where a string makes sense is for ImGui::InputText without using ImGui's stdlib string.

    char slotName[17] = "Player1"; // Slot names cap at 16 characters + terminator
    char slotServer[128] = "archipelago.gg:38281";
    bool hideServer = false;
    char slotPassword[128] = ""; // No password cap?

    char say[256] = ""; // Client -> Server
    std::string APLog = ""; // Various memory management concerns.
    bool APLogCopyMode = false;

    // TODO: ID remaps
    AP_RoomInfo RoomInfo;

    std::string DatapackageChecksum;
    bool datapackageLoaded = false;

    std::filesystem::path LocalPath = std::filesystem::current_path();

    // Datapackage

    nlohmann::json_abi_v3_12_0::json datapackageJSON;
    std::unordered_map<std::string, int64_t> item_name_to_ap_id;
    std::unordered_map<int64_t, std::string> item_ap_id_to_name;
    std::unordered_map<std::string, int64_t> location_name_to_id;
    std::unordered_map<int64_t, std::string> location_id_to_name;

    // Item and location tracking

    std::string DataRequestRaw; // Raw should always be a (JSON) string.
    AP_GetServerDataRequest request;
    bool requested = false; // state tracking of request since it doesn't provide a null state

    nlohmann::json_abi_v3_12_0::json slotData;
    std::vector<int64_t> seedIDs = {}; // Song IDs that are part of the seed
    std::vector<int64_t> recvIDs = {}; // Song IDs received as items
    std::vector<int64_t> missingIDs = {}; // Song IDs not yet received
    std::vector<int64_t> CheckedLocations = {}; // Love is War [1] = 10, 11.

    // TODO: Relocate?
    int clearGrade = 2;
    char diffs[5][10] = {"Cheap", "Standard", "Great", "Excellent", "Perfect"};

    int64_t victoryID = 0; // The AP Item/Loc ID. (confusion ensues)
    int leekHave = 0;
    int leekNeed = 0;

    int &progHPReceived = APDeathLink::HPreceived;
    int &progHPtemp = APDeathLink::HPtemp;
    int &progHPTotal = APDeathLink::HPdenominator;

    void config(const toml::table& settings)
    {
        if (AP_GetConnectionStatus() != AP_ConnectionStatus::Disconnected)
            return;

        toml::table section;
        if (settings.contains("client") && settings["client"].is_table())
            section = *settings["client"].as_table();

        std::string config_name = section["slot_name"].value_or("Player1");
        std::string config_server = section["slot_server"].value_or("archipelago.gg:38281");
        std::string config_pass = section["slot_password"].value_or("");

        std::size_t slotName_len = min(config_name.size(), sizeof(slotName) - 1);
        std::size_t slotServer_len = min(config_server.size(), sizeof(slotServer) - 1);
        std::size_t slotPassword_len = min(config_pass.size(), sizeof(slotPassword) - 1);

        strncpy(slotName, config_name.c_str(), slotName_len);
        strncpy(slotServer, config_server.c_str(), slotServer_len);
        hideServer = section["slot_server_hide"].value_or(false);
        strncpy(slotPassword, config_pass.c_str(), slotPassword_len);

        slotName[slotName_len] = '\0';
        slotServer[slotServer_len] = '\0';
        slotPassword[slotPassword_len] = '\0';
    }

    void save(toml::table& settings)
    {
        toml::table config;
        config.insert("slot_name", slotName);
        config.insert("slot_server", slotServer);
        config.insert("slot_server_hide", hideServer);
        config.insert("slot_password", slotPassword);

        settings.insert("client", config);
    }

    char* getSlotName()
    {
        return slotName;
    }

    void connect()
    {
        AP_Shutdown();

        if (AP_GetConnectionStatus() == AP_ConnectionStatus::Disconnected)
        {
            AP_Init(slotServer, GameName, slotName, slotPassword);

            AP_SetDeathLinkSupported(true);
            AP_SetDeathLinkRecvCallback(RecvDeath);
            //AP_RegisterBouncedCallback(bounced); // Alt function to handle own bounces (death link own slot)

            AP_SetItemClearCallback(ItemClear);
            AP_SetItemRecvCallback(ItemRecv);
            AP_SetLocationCheckedCallback(LocationChecked);

            AP_Start();
        }
    }

    void reset()
    {
        datapackageLoaded = false;

        slotData = nullptr;
        requested = false;
        request.status = AP_RequestStatus::Pending;
        DataRequestRaw.clear();

        seedIDs.clear();
        recvIDs.clear();
        CheckedLocations.clear();

        say[0] = '\0';
        APLog = "";

        clearGrade = 2;
        victoryID = 0;

        leekHave = 0;
        leekNeed = 0;

        progHPReceived = 1;
        progHPtemp = 0;
        progHPTotal = 1;

        APIDHandler::reset();
        APHints::reset();
    }

    // Server messages

    AP_RequestStatus ServerDataRequest_Raw(std::string key, AP_GetServerDataRequest& rawRequest, bool& rawRequested, std::string& output)
    {
        if (!rawRequested)
        {
            rawRequested = true;

            APLogger::print("ServerDataRequest_Raw: %s\n", key.c_str());
            rawRequest.key = key;
            rawRequest.value = &output;
            rawRequest.type = AP_DataType::Raw;
            rawRequest.status = AP_RequestStatus::Pending;

            AP_GetServerData(&rawRequest);
        }

        if (rawRequested && rawRequest.status == AP_RequestStatus::Done || rawRequest.status == AP_RequestStatus::Error)
            rawRequested = false;

        return rawRequest.status;
    }

    void GetSlotData()
    {
        if (slotData.is_null() && request.status == AP_RequestStatus::Pending)
        {
            APLogger::print(__FUNCTION__" pending\n");
            auto name = "_read_slot_data_" + std::to_string(AP_GetPlayerID());
            ServerDataRequest_Raw(name, request, requested, DataRequestRaw);
            return;
        }

        if (!slotData.is_null() || !&DataRequestRaw)
            return;

        // TODO: try catch?

        slotData = nlohmann::json::parse(DataRequestRaw);

        auto final = slotData["finalSongIDs"];
        if (final.is_array())
        {
            seedIDs = final.get<std::vector<int64_t>>();
            std::sort(seedIDs.begin(), seedIDs.end());
        }

        // APCpp provides easy int callbacks, but we're already here...

        victoryID = slotData.value("victoryID", 0);
        clearGrade = slotData.value("scoreGradeNeeded", 2);
        leekNeed = slotData.value("leekWinCount", 1);
        progHPTotal = 1 + slotData.value("progHP", 0); // The value is how many are in the pool, so +1.

        // TODO: Remaps

        DataRequestRaw.clear();
        requested = false;
        APReload::run();
        APTraps::reset();
        UpdateMissing();

        APLogger::print(__FUNCTION__" complete\n");
    }

    void ItemClear()
    {
        APLogger::print("Client: reset\n");
        reset();
    }

    void ItemRecv(int64_t itemID, bool notify)
    {
        switch (itemID) {
            case 1:
                leekHave += 1;
                UpdateMissing();
                break;
            case 2:
                break; // Filler
            case 3:
                APDeathLink::recvHP();
                break;
            case 4:
                APTraps::touchHidden();
                break;
            case 5:
                APTraps::touchSudden();
                break;
            case 9:
                APTraps::touchIcon();
                break;
            default:
                if (itemID >= 10) {
                    PushRecvID(itemID / 10);
                    APHints::updateByItemName(item_ap_id_to_name[itemID]);
                }
        }
    }

    void PushRecvID(int64_t songID)
    {
        if (std::find(recvIDs.begin(), recvIDs.end(), songID) != recvIDs.end())
            return;

        recvIDs.push_back(songID);
        std::sort(recvIDs.begin(), recvIDs.end());

        UpdateMissing();
    }

    void UpdateMissing()
    {
        if (victoryID != 0 && leekHave >= leekNeed)
            PushRecvID(victoryID / 10);

        missingIDs.clear();
        std::set_symmetric_difference(
            seedIDs.begin(), seedIDs.end(),
            recvIDs.begin(), recvIDs.end(),
            std::back_inserter(missingIDs)
        );
    }

    void LocationChecked(int64_t locationID)
    {
        CheckedLocations.push_back(locationID);
    }

    void LocationSend(int64_t pvID)
    {
        if (pvID == victoryID / 10)
        {
            APLogger::print("Client: Sending goal completion from ID %i\n", pvID);
            AP_StoryComplete();
        }
        else {
            APLogger::print("Client: Sending locations for ID %i\n", pvID);

            // Song locations are in pairs
            int64_t APID = pvID * 10;

            std::set<int64_t> locs{ APID, APID + 1 };
            AP_SendItem(locs);

            APHints::updateSentLocations(std::array<int64_t, 2>{ APID, APID + 1});
        }
    }

    void LogAppend(const std::string &text)
    {
        if (APLog.length() > 0)
            APLog += "\n";
        APLog += text;
    }

    void CheckMessages()
    {
        if (AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated)
            return;

        // No potential crashes here.
        LoadDatapackage();
        GetSlotData();

        if (AP_IsMessagePending()) {
            AP_Message* msg = AP_GetLatestMessage();
            APLogger::print("%s\n", msg->text.c_str());

            LogAppend(msg->text);

            if (msg->type == AP_MessageType::Hint)
            {
                AP_HintMessage* h_msg = static_cast<AP_HintMessage*>(msg);
                APHints::handleHintMessage(*h_msg);
            }

            AP_ClearLatestMessage();
        }
    }

    void RecvDeath(std::string src, std::string cause)
    {
        APDeathLink::run(true);
    }

    bool LoadDatapackage()
    {
        // Dynamic datapackage lives on.

        if (datapackageLoaded)
            return true;

        if (AP_GetRoomInfo(&RoomInfo) != 0)
            return false;

        auto it = RoomInfo.datapackage_checksums.find(GameName);
        if (it != RoomInfo.datapackage_checksums.end()) {

            APLogger::print("Datapackage checksum: %s\n", it->second.c_str());

            if (DatapackageChecksum.compare(it->second) != 0)
            {
                APLogger::print("New datapackage checksum\n");
                item_ap_id_to_name.clear();
            }

            DatapackageChecksum = it->second;
        }
        else {
            APLogger::print("Could not find datapackage checksum in RoomInfo\n");
            return false;
        }

        std::ifstream datapackage(LocalPath / ".datapkg-cache" / ("HatsuneMikuProjectDivaMegaMix-" + DatapackageChecksum + ".json"));

        if (!datapackage.is_open())
            return false;

        // TODO: try catch?
        datapackageJSON = nlohmann::json::parse(datapackage);

        item_name_to_ap_id = datapackageJSON["item_name_to_id"].get<std::unordered_map<std::string, int64_t>>();
        for (auto& el : datapackageJSON["item_name_to_id"].items())
            item_ap_id_to_name[(int64_t)el.value()] = el.key();

        location_name_to_id = datapackageJSON["location_name_to_id"].get<std::unordered_map<std::string, int64_t>>();
        for (auto& el : datapackageJSON["location_name_to_id"].items())
            location_id_to_name[(int64_t)el.value()] = el.key();

        datapackageLoaded = true;
        return true;
    }

    void ImGuiTab()
    {
        if (ImGui::BeginTabItem("Client")) {
            if (AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated)
            {
                if (AP_IsInit())
                    ImGui::BeginDisabled();

                ImGui::InputText("Slot Name", slotName, sizeof(slotName));
                ImGui::InputText("Server", slotServer, sizeof(slotServer), !hideServer ? 0 : ImGuiInputTextFlags_Password);
                if (ImGui::BeginPopupContextItem("##hideServer")) {
                    ImGui::Checkbox("Hide", &hideServer);
                    ImGui::EndPopup();
                }

                ImGui::InputText("Password", slotPassword, sizeof(slotPassword), ImGuiInputTextFlags_Password);

                if (AP_IsInit())
                    ImGui::EndDisabled();

                bool disconnected = AP_GetConnectionStatus() == AP_ConnectionStatus::Disconnected;
                bool refused = AP_GetConnectionStatus() == AP_ConnectionStatus::ConnectionRefused;

                if (disconnected || refused)
                    if (!AP_IsInit()) {
                        if (ImGui::Button("Connect"))
                            connect();
                    }
                    else {
                        if (ImGui::Button("Cancel"))
                            AP_Shutdown();
                        ImGui::SameLine();
                        ImGui::Text(refused ? "Wrong Name/Server/Password" : "Connecting...");
                    }
            }
            else
            {
                if (ImGui::Button("Disconnect"))
                {
                    reset();
                    AP_Shutdown();
                }

                ImGui::SameLine();
                ImGui::Text("Connected as %s", slotName);

                ImGui::SameLine();
                if (ImGui::Button("Reload"))
                    APReload::run();

                ImGui::Separator();

                ImGui::BeginChild("APLog", ImVec2(0, 250));

                if (APLogCopyMode) {
                    ImGui::InputTextMultiline("##APLogMulti", (char*)APLog.c_str(), sizeof(APLog), ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_WordWrap);

                    // TODO: dedupe
                    if (ImGui::BeginPopupContextItem("##xx")) {
                        ImGui::Checkbox("Copy mode (no autoscroll)", &APLogCopyMode);
                        ImGui::EndPopup();
                    }
                }
                else {
                    ImGui::PushTextWrapPos(0.0f);

                    std::istringstream stream(APLog);
                    std::string line;

                    while (std::getline(stream, line)) {
                        ImGui::TextUnformatted(line.c_str());
                    }

                    ImGui::PopTextWrapPos();

                    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
                        ImGui::SetScrollHereY(1.0f);
                }

                ImGui::EndChild();

                if (ImGui::BeginPopupContextItem("##xx")) {
                    ImGui::Checkbox("Copy mode (no autoscroll)", &APLogCopyMode);
                    ImGui::EndPopup();
                }

                ImGui::Separator();

                static bool refocus = false;
                if (refocus) {
                    refocus = false;
                    ImGui::SetKeyboardFocusHere();
                }

                if (ImGui::InputText("##APsay", say, sizeof(say), ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    refocus = true;
                    if (strlen(say) > 0) {
                        AP_Say(std::string(say));
                        say[0] = '\0';
                    }
                }

                ImGui::SameLine();
                ImGui::Text("%d / %d Leeks", leekHave, leekNeed);

                // TODO: Relocate
                std::string goalTip = "Goal song: " + item_ap_id_to_name[victoryID] + "\n"
                                      "Clear grade needed: " + (std::string)diffs[clearGrade - 1];

                ImGui::SameLine();
                HelpMarker(goalTip.c_str());
            }

            ImGui::EndTabItem();
        }
    }
}