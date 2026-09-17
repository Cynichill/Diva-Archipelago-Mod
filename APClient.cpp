#include "APClient.h"
#include "APDeathLink.h"
#include "APGUI.h"
#include "APHints.h"
#include "APIDHandler.h"
#include "APReload.h"
#include "APSettings.h"
#include "APTraps.h"
#include <deque>

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
    bool ClientLogCopyMode = false;

    std::deque<AP_Message*> ClientMessages;
    int ClientMessagesMax = 1000;
    bool ClientMessagesFilter_Self = false; // Only show own sends
    bool ClientMessagesFilter_Recv = true;
    bool ClientMessagesFilter_Send = true;
    bool ClientMessagesFilter_Chat = true;
    bool ClientMessagesFilter_Server = true;
    bool ClientMessagesFilter_Hint = true;
    bool ClientMessagesFilter_Countdown = true;
    bool ClientMessagesFilter_Plain = true;

    ImVec4 ClientMessagesColor_Player = ImColor(237, 0, 237, 255);
    ImVec4 ClientMessagesColor_Others = ImColor(249, 249, 209, 255);

    ImVec4 ClientMessagesColor_Progression = ImColor(173, 153, 239, 255);
    ImVec4 ClientMessagesColor_Useful = ImColor(107, 137, 229, 255);
    ImVec4 ClientMessagesColor_Trap = ImColor(249, 127, 112, 255);
    ImVec4 ClientMessagesColor_Filler = ImColor(0, 237, 237, 255);

    // Hold server data messaging
    std::vector<std::pair<AP_GetServerDataRequest, std::function<void(std::string raw)>>> DataRequests;

    // Datapackage
    std::string DatapackageChecksum;
    bool datapackageLoaded = false;

    json datapackageJSON;
    std::unordered_map<std::string, int64_t> item_name_to_ap_id;
    std::unordered_map<int64_t, std::string> item_ap_id_to_name;
    std::unordered_map<std::string, int64_t> location_name_to_id;
    std::unordered_map<int64_t, std::string> location_id_to_name;

    // Archipelago state

    AP_RoomInfo RoomInfo;

    std::vector<int64_t> seedIDs = {}; // Song IDs (Love is War [1] = 1) that are part of the seed
    std::vector<int64_t> recvIDs = {}; // Song IDs (Love is War [1] = 1) received as items
    std::vector<int64_t> missingIDs = {}; // Song IDs (Love is War [1] = 1) not yet received
    std::vector<int64_t> CheckedLocations = {}; // Love is War [1] = AP_ID_FACTOR, AP_ID_FACTOR+1

    int64_t victoryID = 0; // Song ID * AP_ID_FACTOR, Love is War [1] = AP_ID_FACTOR
    int leekHave = 0;
    int leekNeed = 0;
    int locHave = 0;
    int locNeed = 0;
    int clearGrade = 2;
    char diffs[5][10] = { "Cheap", "Standard", "Great", "Excellent", "Perfect" }; // TODO: Relocate?

    int &progHPReceived = APDeathLink::HPreceived;
    int &progHPtemp = APDeathLink::HPtemp;
    int &progHPTotal = APDeathLink::HPdenominator;

    void config(const toml::table& settings)
    {
        toml::table section;
        if (settings.contains("client") && settings["client"].is_table())
            section = *settings["client"].as_table();

        // Message filters

        ClientMessagesFilter_Chat = section["show_chat"].value_or(ClientMessagesFilter_Chat);
        ClientMessagesFilter_Recv = section["show_countdown"].value_or(ClientMessagesFilter_Recv);
        ClientMessagesFilter_Hint = section["show_hint"].value_or(ClientMessagesFilter_Hint);
        ClientMessagesFilter_Recv = section["show_recv"].value_or(ClientMessagesFilter_Recv);
        ClientMessagesFilter_Send = section["show_send"].value_or(ClientMessagesFilter_Send);
        ClientMessagesFilter_Self = section["show_send_self"].value_or(ClientMessagesFilter_Self);
        ClientMessagesFilter_Server = section["show_server"].value_or(ClientMessagesFilter_Server);
        ClientMessagesFilter_Plain = section["show_plain"].value_or(ClientMessagesFilter_Plain);

        // Colors

        ClientMessagesColor_Player = ImColor(section["color_player"].value_or(ImGui::ColorConvertFloat4ToU32(ClientMessagesColor_Player)));
        ClientMessagesColor_Others = ImColor(section["color_others"].value_or(ImGui::ColorConvertFloat4ToU32(ClientMessagesColor_Others)));

        ClientMessagesColor_Progression = ImColor(section["color_progression"].value_or(ImGui::ColorConvertFloat4ToU32(ClientMessagesColor_Progression)));
        ClientMessagesColor_Useful = ImColor(section["color_useful"].value_or(ImGui::ColorConvertFloat4ToU32(ClientMessagesColor_Useful)));
        ClientMessagesColor_Trap = ImColor(section["color_trap"].value_or(ImGui::ColorConvertFloat4ToU32(ClientMessagesColor_Trap)));
        ClientMessagesColor_Filler = ImColor(section["color_filler"].value_or(ImGui::ColorConvertFloat4ToU32(ClientMessagesColor_Filler)));

        if (AP_GetConnectionStatus() != AP_ConnectionStatus::Disconnected)
            return;

        // Connection info

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

        // Message filters

        config.insert("show_chat", ClientMessagesFilter_Chat);
        config.insert("show_countdown", ClientMessagesFilter_Recv);
        config.insert("show_hint", ClientMessagesFilter_Hint);
        config.insert("show_recv", ClientMessagesFilter_Recv);
        config.insert("show_send", ClientMessagesFilter_Send);
        config.insert("show_send_self", ClientMessagesFilter_Self);
        config.insert("show_server", ClientMessagesFilter_Server);
        config.insert("show_plain", ClientMessagesFilter_Plain);

        // Colors

        config.insert("color_player", ImGui::ColorConvertFloat4ToU32(ClientMessagesColor_Player));
        config.insert("color_others", ImGui::ColorConvertFloat4ToU32(ClientMessagesColor_Others));

        config.insert("color_progression", ImGui::ColorConvertFloat4ToU32(ClientMessagesColor_Progression));
        config.insert("color_useful", ImGui::ColorConvertFloat4ToU32(ClientMessagesColor_Useful));
        config.insert("color_trap", ImGui::ColorConvertFloat4ToU32(ClientMessagesColor_Trap));
        config.insert("color_filler", ImGui::ColorConvertFloat4ToU32(ClientMessagesColor_Filler));

        settings.insert("client", config);
    }

    char* getSlotName()
    {
        return slotName;
    }

    void SlotData_LeekWin(int leekWinCount)
    {
        leekNeed = leekWinCount;
    }

    void SlotData_LocWin(int locWinCount)
    {
        locNeed = locWinCount;
    }

    void SlotData_ProgHP(int progHP)
    {
        progHPTotal = 1 + progHP;
    }

    void SlotData_Grade(int grade)
    {
        clearGrade = grade;
    }

    void SlotData_VictoryID(int id)
    {
        victoryID = id;
    }

    void SlotData_FinalSongs(std::string raw)
    {
        auto final = json::parse(raw);
        if (final.is_array())
        {
            seedIDs = final.get<std::vector<int64_t>>();
            std::sort(seedIDs.begin(), seedIDs.end());
        }

        ImGui::SetWindowFocus("Client");
        UpdateMissing();
        UpdateTags();
        APReload::run();
        APTraps::reset();
    }

    void ItemClear()
    {
        APLogger::print("Client: reset\n");
        reset();
    }

    void RecvBounce(AP_Bounce bouncePacket)
    {
        if (bouncePacket.tags == nullptr) return;

        json data = json::parse(bouncePacket.data);

        if (bouncePacket.tags->front() == "TrapLink") {
            std::string src = data.value("source", "");

            if (src.empty() || src == std::string(getSlotName()))
                return;

            std::string trap = data.value("trap_name", "");
            APTraps::linkRecv(trap);
        }
        else if (bouncePacket.tags->front() == "DeathLink") {
            RecvDeath(data.value("source", ""), data.value("cause", ""));
        }
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
        default:
            if (APTraps::canRecv(itemID)) {
                APTraps::trapRecv(itemID, notify);
            }
            else if (itemID >= AP_ID_FACTOR) {
                PushRecvID(itemID / AP_ID_FACTOR);
                APHints::updateByItemName(item_ap_id_to_name[itemID]);
            }
        }

        APIDHandler::refreshTracker();
    }

    void LocationChecked(int64_t locationID)
    {
        if (std::ranges::find(CheckedLocations, locationID) != CheckedLocations.end())
            return;

        CheckedLocations.push_back(locationID);
        UpdateMissing();
        APIDHandler::refreshTracker();
        // TODO: UpdateMissing's goal check relies on locHave which is managed by the tracker
        UpdateMissing();
    }

    void connect()
    {
        AP_Shutdown();

        if (AP_GetConnectionStatus() == AP_ConnectionStatus::Disconnected)
        {
            AP_Init(slotServer, GameName, slotName, slotPassword);
            AP_RegisterBouncedCallback(RecvBounce);

            AP_SetItemClearCallback(ItemClear);
            AP_SetItemRecvCallback(ItemRecv);
            AP_SetLocationCheckedCallback(LocationChecked);

            AP_RegisterSlotDataIntCallback("victoryID", SlotData_VictoryID);
            AP_RegisterSlotDataIntCallback("scoreGradeNeeded", SlotData_VictoryID);
            AP_RegisterSlotDataIntCallback("leekWinCount", SlotData_LeekWin);
            AP_RegisterSlotDataIntCallback("locWinCount", SlotData_LocWin);
            AP_RegisterSlotDataIntCallback("progHP", SlotData_ProgHP);
            AP_RegisterSlotDataRawCallback("finalSongIDs", SlotData_FinalSongs);

            AP_Start();
        }
    }

    void pushClientMessage(AP_Message* msg)
    {
        if (
            msg->type == AP_MessageType::Countdown && !ClientMessagesFilter_Countdown ||
            msg->type == AP_MessageType::ServerChat && !ClientMessagesFilter_Server ||
            msg->type == AP_MessageType::Chat && !ClientMessagesFilter_Chat ||
            msg->type == AP_MessageType::Hint && !ClientMessagesFilter_Hint ||
            msg->type == AP_MessageType::Plaintext && !ClientMessagesFilter_Plain
            )
            return;


        while (ClientMessages.size() > ClientMessagesMax) {
            auto front = ClientMessages.front();
            delete front;

            ClientMessages.pop_front();
        }

        ClientMessages.push_back(msg);
    }

    void clearClientMessages()
    {
        for (auto i : ClientMessages)
            delete i;

        ClientMessages.clear();
    }

    void reset()
    {
        datapackageLoaded = false;

        DataRequests.clear();

        seedIDs.clear();
        recvIDs.clear();
        missingIDs.clear();
        CheckedLocations.clear();

        say[0] = '\0';
        clearClientMessages();

        clearGrade = 2;
        victoryID = 0;

        leekHave = 0;
        leekNeed = 0;
        locHave = 0;
        locNeed = 0;

        progHPReceived = 1;
        progHPtemp = 0;
        progHPTotal = 1;

        APIDHandler::reset();
        APHints::reset();
    }

    void PushRecvID(int64_t songID)
    {
        if (std::ranges::find(recvIDs, songID) != recvIDs.end() ||
            std::ranges::find(seedIDs, songID) == seedIDs.end())
            return;

        recvIDs.push_back(songID);

        UpdateMissing();
    }

    void UpdateMissing()
    {
        if (victoryID >= AP_ID_FACTOR && (leekNeed > 0 && leekHave >= leekNeed) || (locNeed > 0 && locHave >= locNeed))
            PushRecvID(victoryID / AP_ID_FACTOR);

        // TODO: Works from a copy to preserve receive order for the Tracker.
        // Tracking the order can be moved higher to APClient::ItemRecv.
        // set_symmetric_difference items need to be presorted. If missingIDs is wrong Freeplay breaks.
        auto _recvIDs = recvIDs;
        std::sort(_recvIDs.begin(), _recvIDs.end());

        missingIDs.clear();
        std::set_symmetric_difference(
            seedIDs.begin(), seedIDs.end(),
            _recvIDs.begin(), _recvIDs.end(),
            std::back_inserter(missingIDs)
        );
    }

    void LocationSend(int64_t pvID)
    {
        // There is no current way to send an arbitrary ID so limit to received ones. Usually what's on the Tracker.
        // Specifically to prevent misfires of the AP and Tutorial songs but may benefit Freeplay.
        if (std::ranges::find(recvIDs, pvID) == recvIDs.end() /*&& !devMode*/) {
            APLogger::print("Client: Skip location send for ID %i (not received)\n", pvID);
            return;
        }

        if (pvID == victoryID / AP_ID_FACTOR)
        {
            APLogger::print("Client: Sending goal completion from ID %i\n", pvID);
            AP_StoryComplete();
        }
        else {
            APLogger::print("Client: Sending locations for ID %i\n", pvID);

            // Song locations are in pairs
            int64_t APID = pvID * AP_ID_FACTOR;

            std::set<int64_t> locs{ APID, APID + 1 };
            AP_SendItem(locs);

            APHints::updateSentLocations(std::array<int64_t, 2>{ APID, APID + 1});
            UpdateMissing();
        }
    }

    // Server messages

    void DataRequest(const std::string key, std::function<void(std::string raw)> callback)
    {
        std::pair<AP_GetServerDataRequest, std::function<void(std::string raw)>> pair;

        AP_GetServerDataRequest req;
        req.key = key;
        req.value = new std::string;
        req.type = AP_DataType::Raw;
        req.status = AP_RequestStatus::Pending;

        pair.first = req;
        pair.second = callback;

        DataRequests.push_back(pair);

        AP_GetServerData(&DataRequests.back().first);
    }

    void CheckMessages()
    {
        if (AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated)
            return;

        if (DataRequests.size() > 0) {
            for (auto &[req, callback] : DataRequests) {
                if (req.status == AP_RequestStatus::Pending) // What about stuck pending?
                    continue;

                //if (req.status == AP_RequestStatus::Error)

                if (req.status == AP_RequestStatus::Done) {
                    std::string value = *(std::string*)req.value;
                    if (req.type == AP_DataType::Raw)
                        callback(value);
                }

                free(req.value);
                DataRequests.clear(); // TODO: conditional remove
            }
        }

        // No potential crashes here.
        LoadDatapackage();

        if (AP_IsMessagePending()) {
            AP_Message* msg = AP_GetLatestMessage();

            if (msg->type == AP_MessageType::ItemRecv) {
                auto msg_recv = static_cast<AP_ItemRecvMessage*>(msg);
                auto msg_push = new AP_ItemRecvMessage(*msg_recv);
                pushClientMessage(msg_push);
            }
            else if (msg->type == AP_MessageType::ItemSend) {
                auto msg_send = static_cast<AP_ItemSendMessage*>(msg);
                auto msg_push = new AP_ItemSendMessage(*msg_send);
                pushClientMessage(msg_push);
            }
            else if (msg->type == AP_MessageType::Hint)
            {
                auto msg_hint = static_cast<AP_HintMessage*>(msg);
                auto msg_push = new AP_HintMessage(*msg_hint);
                pushClientMessage(msg_push);

                APHints::handleHintMessage(*msg_hint);
            }
            else {
                auto msg_push = new AP_Message(*msg);
                //msg_push->type = AP_MessageType::Plaintext;
                //msg_push->text = msg->text;
                pushClientMessage(msg_push);
            }

            APLogger::print("%s\n", msg->text.c_str());

            AP_ClearLatestMessage();
        }
    }

    void RecvDeath(const std::string& src, const std::string& cause)
    {
        auto msg = new AP_Message;
        msg->text = cause.empty() ? src + " died" : cause;
        pushClientMessage(msg);

        if (src == slotName && !APDeathLink::death_link_self) return;
        APDeathLink::run(true);
    }

    void UpdateTags()
    {
        if (AP_GetConnectionStatus() == AP_ConnectionStatus::Disconnected)
            return;

        std::vector<std::string> tags;

        if (APDeathLink::death_link)
            tags.insert(tags.end(), APDeathLink::death_link_tags.begin(), APDeathLink::death_link_tags.end());

        if (APTraps::trap_link)
            tags.insert(tags.end(), APTraps::trap_link_tags.begin(), APTraps::trap_link_tags.end());

        AP_UpdateTags(tags);
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

        std::ifstream datapackage(BasePath / ".datapkg-cache" / ("HatsuneMikuProjectDivaMegaMix-" + DatapackageChecksum + ".json"));

        if (!datapackage.is_open())
            return false;

        // TODO: try catch?
        datapackageJSON = json::parse(datapackage);

        item_name_to_ap_id = datapackageJSON["item_name_to_id"].get<std::unordered_map<std::string, int64_t>>();
        for (auto& el : datapackageJSON["item_name_to_id"].items())
            item_ap_id_to_name[(int64_t)el.value()] = el.key();

        location_name_to_id = datapackageJSON["location_name_to_id"].get<std::unordered_map<std::string, int64_t>>();
        for (auto& el : datapackageJSON["location_name_to_id"].items())
            location_id_to_name[(int64_t)el.value()] = el.key();

        int _AP_ID_FACTOR = (int)(std::pow(10, (int)(log10(item_name_to_ap_id["Love is War [1]"]))));
        if (_AP_ID_FACTOR != AP_ID_FACTOR) {
            APLogger::print("AP_ID_FACTOR changed from %i to %i\n", AP_ID_FACTOR, _AP_ID_FACTOR);
            AP_ID_FACTOR = _AP_ID_FACTOR;
        }

        datapackageLoaded = true;

        return true;
    }

    ImVec4* flagsToColor(const int flags)
    {
        if (flags & static_cast<int>(AP_ItemFlags::Advancement))
            return &ClientMessagesColor_Progression;
        else if (flags & static_cast<int>(AP_ItemFlags::Useful))
            return &ClientMessagesColor_Useful;
        else if (flags & static_cast<int>(AP_ItemFlags::Trap))
            return &ClientMessagesColor_Trap;

        return &ClientMessagesColor_Filler;
    }

    void RichTextWrap(const std::vector<std::pair<ImVec4*, std::string>> &parts)
    {
        for (auto& [color, str] : parts) {
            if (color != nullptr)
                ImGui::PushStyleColor(ImGuiCol_Text, *color);

            auto words = std::views::split(str, ' ') | std::views::transform([](auto&& subrange) {
                return std::string(subrange.begin(), subrange.end());
                });

            for (auto it = words.begin(); it != words.end(); ++it) {
                const auto& word = *it;
                auto avail = ImGui::GetContentRegionAvail().x;
                auto size = ImGui::CalcTextSize((word + " ").c_str()).x;

                if (size > avail)
                    ImGui::Spacing();

                ImGui::Text(word.c_str());

                const auto& nextword = *std::next(it);
                auto nextavail = ImGui::GetContentRegionAvail().x;
                auto nextsize = ImGui::CalcTextSize((nextword + " ").c_str()).x;

                if (nextsize <= nextavail)
                    ImGui::SameLine();
            }

            if (color != nullptr)
                ImGui::PopStyleColor();
        }

        ImGui::Spacing();
    }

    void ImGuiTab()
    {
        if (AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated)
        {
            if (AP_IsInit())
                ImGui::BeginDisabled();

            ImGui::InputText("Slot Name", slotName, sizeof(slotName));
            ImGui::InputText("Server", slotServer, sizeof(slotServer), !hideServer ? 0 : ImGuiInputTextFlags_Password);
            if (ImGui::BeginPopupContextItem("##hideServer")) {
                ImGui::MenuItem("Hide server", nullptr, &hideServer);
                ImGui::EndPopup();
            }
            HelpMarker(
                "Server address must have the port number.\nRight-click input to toggle visibility."
                "\n\nExample addresses:\n archipelago.gg:38281\n localhost:38281\n 127.0.0.1:38281"
            );

            ImGui::InputText("Password", slotPassword, sizeof(slotPassword), ImGuiInputTextFlags_Password);

            if (AP_IsInit())
                ImGui::EndDisabled();

            bool disconnected = AP_GetConnectionStatus() == AP_ConnectionStatus::Disconnected;
            bool refused = AP_GetConnectionStatus() == AP_ConnectionStatus::ConnectionRefused;

            if (disconnected || refused)
                if (!AP_IsInit()) {
                    const int* state = (int*)0x14CC61078;

                    if (*state == 0) ImGui::BeginDisabled();
                    if (ImGui::Button("Connect")) {
                        connect();
                        if (ImGui::GetIO().KeyShift)
                            APSettings::save();
                    }
                    HelpMarker("Shift+Click to save connection information.");
                    if (*state == 0) ImGui::EndDisabled();
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
            if (ImGui::Button("Disconnect")) {
                AP_Shutdown();
                reset();

                if (!ImGui::GetIO().KeyShift)
                    APReload::run();
            }
            HelpMarker("Shift+Click to not reload.");

            ImGui::SameLine();
            ImGui::Text("Connected as %s", slotName);

            ImGui::SameLine();
            if (ImGui::Button("Reload"))
                APReload::run();

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Reload key: %s", APReload::reloadVal);
                ImGui::EndTooltip();
            }

            ImGui::Separator();

            ImGui::BeginChild("ClientLog", ImVec2(0, ImGui::GetContentRegionAvail().y - (ImGui::GetFrameHeightWithSpacing() * 1.2f)));

            ImGui::BeginChild("APLogUnformatted");

            for (auto msg : ClientMessages) {
                if (msg->type == AP_MessageType::ItemSend) {
                    if (!ClientMessagesFilter_Send) continue;

                    auto msg_send = static_cast<AP_ItemSendMessage*>(msg);
                    bool isSlotSend = APHints::isPlayer(msg_send->sendPlayer);
                    if (ClientMessagesFilter_Self && !isSlotSend) continue;

                    bool isSame = msg_send->sendPlayer == msg_send->recvPlayer;
                    bool isSlotRecv = APHints::isPlayer(msg_send->recvPlayer);

                    std::vector<std::pair<ImVec4*, std::string>> parts = {
                        { isSlotSend ? &ClientMessagesColor_Player : &ClientMessagesColor_Others, msg_send->sendPlayer },
                        { nullptr, std::string(isSame ? "found their" : "sent") },
                        { flagsToColor(msg_send->flags), msg_send->item },
                    };

                    if (!isSame) {
                        parts.push_back({ nullptr, std::string("to") });
                        parts.push_back({ isSlotRecv ? &ClientMessagesColor_Player : &ClientMessagesColor_Others, msg_send->recvPlayer });
                    }

                    RichTextWrap(parts);
                }
                else if (msg->type == AP_MessageType::ItemRecv) {
                    if (!ClientMessagesFilter_Recv) continue;

                    auto msg_recv = static_cast<AP_ItemRecvMessage*>(msg);
                    bool isSlot = APHints::isPlayer(msg_recv->sendPlayer);

                    std::vector<std::pair<ImVec4*, std::string>> parts = {
                        { isSlot ? &ClientMessagesColor_Player : &ClientMessagesColor_Others, msg_recv->sendPlayer },
                        { nullptr, std::string(isSlot ? "found their" : "sent") },
                        { flagsToColor(msg_recv->flags), msg_recv->item },
                    };

                    if (!isSlot) {
                        parts.push_back({ nullptr, std::string("to") });
                        parts.push_back({ &ClientMessagesColor_Player, std::string(getSlotName()) });
                    }

                    RichTextWrap(parts);
                }
                else {
                    ImGui::TextWrapped(msg->text.c_str());
                }
            }

            static bool atBottom;
            atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
            if (atBottom) ImGui::SetScrollHereY(1.0f);

            ImGui::EndChild();

            if (ImGui::BeginPopupContextItem("##xx")) {
                if (ImGui::BeginMenu("Show message types")) {
                    ImGui::MenuItem("Only show relevant sends", nullptr, &ClientMessagesFilter_Self);
                    ImGui::Separator();
                    ImGui::MenuItem("Item sent", nullptr, &ClientMessagesFilter_Send);
                    ImGui::MenuItem("Item received", nullptr, &ClientMessagesFilter_Recv);
                    ImGui::MenuItem("Hints", nullptr, &ClientMessagesFilter_Hint);
                    ImGui::MenuItem("Chat", nullptr, &ClientMessagesFilter_Chat);
                    ImGui::MenuItem("Server chat", nullptr, &ClientMessagesFilter_Server);
                    ImGui::MenuItem("Countdown", nullptr, &ClientMessagesFilter_Countdown);
                    ImGui::MenuItem("Plain", nullptr, &ClientMessagesFilter_Plain);

                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Colors")) {
                    ImGui::ColorEdit4(getSlotName(), (float*)&ClientMessagesColor_Player, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                    ImGui::ColorEdit4("Others##xx", (float*)&ClientMessagesColor_Others, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);

                    ImGui::Separator();

                    ImGui::ColorEdit4("Progression", (float*)&ClientMessagesColor_Progression, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                    ImGui::ColorEdit4("Useful", (float*)&ClientMessagesColor_Useful, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                    ImGui::ColorEdit4("Trap", (float*)&ClientMessagesColor_Trap, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                    ImGui::ColorEdit4("Filler", (float*)&ClientMessagesColor_Filler, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);

                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Clear")) clearClientMessages();
                if (devMode) {
                    ImGui::BeginDisabled();
                    ImGui::Text("%i / %i", ClientMessages.size(), ClientMessagesMax);
                    ImGui::EndDisabled();
                }
                ImGui::EndPopup();
            }

            ImGui::EndChild();

            ImGui::Separator();

            static bool refocus = false;
            if (refocus) {
                refocus = false;
                ImGui::SetKeyboardFocusHere();
            }

            std::string winCon;
            if (leekNeed > 0)
                winCon = std::format("{} / {} Leeks", leekHave, leekNeed);
            else if (locNeed > 0)
                winCon = std::format("{} / {} Checks", locHave, locNeed);

            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize((winCon + (APGUI::inlineTooltips ? " " : " (?) ")).c_str()).x);
            if (ImGui::InputText("##APsay", say, sizeof(say), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                refocus = true;
                if (strlen(say) > 0) {
                    AP_Say(std::string(say));
                    say[0] = '\0';
                }
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            ImGui::Text(winCon.c_str());

            // TODO: Relocate
            std::string goalTip = std::format("Goal song: {}\nClear grade needed: {}",
                                               item_ap_id_to_name[victoryID], diffs[clearGrade - 1]);

            HelpMarker(goalTip.c_str());
        }
    }
}