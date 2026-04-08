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
    char slotPassword[128] = ""; // No password cap?

    char say[256] = ""; // Client -> Server
    std::string APLog = ""; // Various memory management concerns.

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

    void config(toml::v3::ex::parse_result& data)
    {
        std::string config_name = data["slot_name"].value_or("Player1");
        std::string config_server = data["slot_server"].value_or("archipelago.gg:38281");
        std::string config_pass = data["slot_password"].value_or("");

        strncpy(slotName, config_name.c_str(), config_name.size() + 1);
        strncpy(slotServer, config_server.c_str(), config_server.size() + 1);
        strncpy(slotPassword, config_pass.c_str(), config_pass.size() + 1);
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

            // Requires `death_link` in slot data, not `deathLink`, etc.
            AP_SetDeathLinkSupported(true);
            //AP_RegisterBouncedCallback(bounced);
            AP_SetDeathLinkRecvCallback(RecvDeath); // Switch to Bounce callback handle

            AP_SetItemClearCallback(ItemClear);
            AP_SetItemRecvCallback(ItemRecv);
            AP_SetLocationCheckedCallback(LocationChecked);

            // deathLink -> death_link type? / Needs a rename for APCpp, use Bounce callback, always enabled in slot data

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
                progHPtemp = 0;
                progHPReceived += 1;
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
            AP_StoryComplete();
        }
        else {
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

    void SendDeath()
    {
        if (APDeathLink::deathLinked)
            return;

        // TODO: Slot aliases?
        std::string msg = "The Disappearance of " + (std::string)slotName;

        AP_DeathLinkSend(msg);
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

            if (DatapackageChecksum.compare(it->second))
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
                ImGui::InputText("Slot Name", slotName, sizeof(slotName));
                ImGui::InputText("Server", slotServer, sizeof(slotServer));
                ImGui::InputText("Password", slotPassword, sizeof(slotPassword), ImGuiInputTextFlags_Password);

                if (ImGui::Button("Connect"))
                    connect();

                ImGui::SameLine();
                if (AP_GetConnectionStatus() == AP_ConnectionStatus::ConnectionRefused)
                    ImGui::Text("Wrong Name/Server/Password");

                if (AP_GetConnectionStatus() == AP_ConnectionStatus::Connected)
                    ImGui::Text("Connected");
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

                // TODO: Switch to TextWrapped, smart auto scroll to bottom
                ImGui::InputTextMultiline("##APlog", (char *)APLog.c_str(), sizeof(APLog), ImVec2(ImGui::GetContentRegionAvail().x, 0), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_WordWrap);
                ImGui::SetScrollHereY(1.0f);

                static bool refocus = false;
                if (refocus) {
                    refocus = false;
                    ImGui::SetKeyboardFocusHere();
                }

                if (ImGui::InputText("##APsay", say, sizeof(say), ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    AP_Say(std::string(say));
                    say[0] = '\0';

                    refocus = true;
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