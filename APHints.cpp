#include "APClient.h"
#include "APHints.h"

namespace APHints
{
    // Currently missing most of the networking to do hints properly
    // The potential expensive operations are !hint refreshes due to server notices instead of JSON
    // Hopefully in the future use _read_hints_{team}_{slot}
    //  https://github.com/ArchipelagoMW/Archipelago/blob/main/docs/network%20protocol.md#get
    //  https://github.com/ArchipelagoMW/Archipelago/blob/main/docs/network%20protocol.md#Hint

    bool devMode = APClient::devMode;

    bool init = false; // If first !hint has been sent
    bool hintHideChecked = true;
    bool hintOwnLocationsOnly = false;

    // For updating known hints without further !hint chats (and saving on PrintJSONs)
    std::string hintsRaw_S; // Request response, JSON in a string
    AP_GetServerDataRequest hintsRequest;
    bool hintsRequested = false; // actually state track if the request is known

    // AP_HintMessage passes players as stringified names instead of player ID. Good and bad.
    std::vector<AP_HintMessage> Hints;
    std::vector<int64_t> HintedIDs;

    auto& recvIDs = APClient::recvIDs;
    auto& item_name_to_ap_id = APClient::item_name_to_ap_id;
    auto& item_ap_id_to_name = APClient::item_ap_id_to_name;
    auto& location_name_to_id = APClient::location_name_to_id;
    auto& location_id_to_name = APClient::location_id_to_name;

    bool operator==(const AP_HintMessage& hintA, const AP_HintMessage& hintB)
    {
        // Exclude "checked"
        return (hintA.item == hintB.item && hintA.location == hintB.location
                && hintA.sendPlayer == hintB.sendPlayer && hintA.recvPlayer == hintB.recvPlayer);
    }

    void reset()
    {
        init = false;
        hintsRequested = false;
        Hints.clear();
        HintedIDs.clear();
    }

    void handleHintMessage(const AP_HintMessage& recvHint)
    {
        if (recvHint.sendPlayer != APClient::getSlotName() && recvHint.recvPlayer != APClient::getSlotName())
            return;

        for (AP_HintMessage &hint : Hints)
        {
            if (hint == recvHint)
            {
                hint.checked = recvHint.checked;
                return;
            }
        }

        if (recvHint.sendPlayer.compare(APClient::getSlotName()) == 0)
        {
            // TODO: ID Remaps
            auto itemID = location_name_to_id[recvHint.location] / 10;

            if (std::find(HintedIDs.begin(), HintedIDs.end(), itemID) == HintedIDs.end())
                HintedIDs.push_back(itemID);
        }

        Hints.push_back(recvHint);
    }

    void refreshHints()
    {
        if (!hintsRequested) {
            auto name = "_read_hints_0_" + std::to_string(AP_GetPlayerID());
            APClient::ServerDataRequest_Raw(name, hintsRequest, hintsRequested, hintsRaw_S);
            return;
        }

        if (hintsRequest.status != AP_RequestStatus::Error)
        {
            hintsRequested = false;
            APLogger::print(__FUNCTION__": error\n");
            return;
        }

        if (hintsRequest.status == AP_RequestStatus::Done && hintsRaw_S.empty())
        {
            hintsRequested = false;
            APLogger::print(__FUNCTION__": request returned empty, abort\n");
            return;
        }

        nlohmann::json tempHints;

        try {
            nlohmann::json tempHints = nlohmann::json::parse(hintsRaw_S);

            for (const auto& hint : tempHints) {
                if (hint["status"] != 40 || hint["item"] < 10 || hint["receiving_player"] != AP_GetPlayerID())
                    continue;

                auto itemName = item_ap_id_to_name[hint["item"]];

                for (AP_HintMessage& hint : Hints) {
                    if (hint.item.compare(itemName) == 0)
                        hint.checked = true;
                }
            }
        }
        catch (const nlohmann::json::parse_error& e) {
            APLogger::print(__FUNCTION__": JSON parse error: (%d) %s\n", e.byte, e.what());
        }
        catch (const std::exception& e) {
            APLogger::print(__FUNCTION__": Exception during JSON parse: %s\n", e.what());
        }

        hintsRequested = false;
        hintsRaw_S.clear();
    }

    void updateSentLocations(const std::array<int64_t, 2> locationIDs)
    {
        // Simply no better way...
        for (const auto &locationID : locationIDs) {
            auto location = location_id_to_name[locationID];
            for (auto &hint : Hints) {
                if (hint.sendPlayer.compare(APClient::getSlotName()) == 0 && location.compare(hint.location) == 0)
                    hint.checked = true;
            }
        }
    }

    void updateByItemName(const std::string itemName)
    {
        if (item_name_to_ap_id[itemName] < 10)
            return; // Without location data, good luck. Dupes make some sense at least.

        for (auto& hint : Hints) {
            if (hint.recvPlayer.compare(APClient::getSlotName()) == 0 && hint.item.compare(itemName) == 0) {
                APLogger::print(__FUNCTION__" setting checked for %s\n", hint.location.c_str());
                hint.checked = true;
            }
        }
    }

    void ImGuiTab()
    {
        //if (!APClient::devMode) return;

        if (ImGui::BeginTabItem("Hints")) {
            if (!init) {
                AP_Say("!hint");
                init = true;
            }

            if (hintsRequested)
                refreshHints();

            ImGui::Checkbox("Hide checked", &hintHideChecked);
            ImGui::SameLine();
            HelpMarker("Non-song items may be out of date until manually refreshed.\n");
            ImGui::SameLine();

            float avail = ImGui::GetContentRegionAvail().x;
            float buttonWidth = ImGui::CalcTextSize("Manual Refresh").x + ImGui::GetStyle().FramePadding.x * 2;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - buttonWidth);

            if (ImGui::Button("Manual Refresh"))
                refreshHints();

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Refresh checked status for non-song items.\n");
                ImGui::EndTooltip();
            }

            ImGui::Checkbox("Only show my checks", &hintOwnLocationsOnly);

            ImGui::SameLine();
            std::string hintLabel = std::to_string(Hints.size()) + " Hints";
            avail = ImGui::GetContentRegionAvail().x;
            buttonWidth = ImGui::CalcTextSize(hintLabel.c_str()).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - buttonWidth);
            ImGui::Text("%s", hintLabel.c_str());

            if (ImGui::BeginChild("tableHintsContainer", ImVec2(0, 300))) {
                if (ImGui::BeginTable("tableHints", 5,
                    ImGuiTableFlags_BordersInner | ImGuiTableFlags_Hideable | ImGuiTableFlags_HighlightHoveredColumn |
                    ImGuiTableFlags_Reorderable | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit
                ))
                {
                    ImGui::TableSetupColumn(" ");
                    ImGui::TableSetupColumn("Finder");
                    ImGui::TableSetupColumn("Receiver");
                    ImGui::TableSetupColumn("Item");
                    ImGui::TableSetupColumn("Location");
                    ImGui::TableHeadersRow();

                    int uid = 0;
                    for (const AP_HintMessage& hint : Hints) {
                        if (hintHideChecked && hint.checked)
                            continue;

                        bool isMyCheck = hint.sendPlayer.compare(APClient::getSlotName()) == 0;

                        if (hintOwnLocationsOnly && !isMyCheck)
                            continue;

                        ImGui::TableNextRow();

                        uid += 1;
                        ImGui::PushID(uid);

                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s", hint.checked ? "X" : " ");

                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%s", hint.sendPlayer.c_str());

                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%s", hint.recvPlayer.c_str());

                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%s", hint.item.c_str());

                        ImGui::TableSetColumnIndex(4);

                        // TODO: ID Remaps
                        auto locID = location_name_to_id[hint.location.c_str()];
                        auto itemName = item_ap_id_to_name[(locID / 10) * 10];

                        bool haveItem = isMyCheck && std::find(recvIDs.begin(), recvIDs.end(), locID / 10) != recvIDs.end();

                        if (haveItem)
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));

                        ImGui::Text("%s", hint.location.c_str());

                        if (haveItem)
                            ImGui::PopStyleColor();

                        if (isMyCheck && !haveItem) {
                            if (ImGui::BeginPopupContextItem("##xx")) {
                                if (ImGui::MenuItem("Hint this song##xx"))
                                    AP_Say("!hint " + itemName);

                                ImGui::EndPopup();
                            }
                        }

                        ImGui::PopID();
                    }

                    ImGui::EndTable();
                }

                ImGui::EndChild();
            }

            ImGui::EndTabItem();
        }
    }
}