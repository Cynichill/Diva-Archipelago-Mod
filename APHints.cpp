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

    bool resortHints = false; // If true, run a sort the next time the Hint table is visible.

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
        drop();
    }

    void drop()
    {
        Hints.clear();
        HintedIDs.clear();
    }

    bool isPlayer(const std::string &playerName)
    {
        // The current API-less hint support crumbles when aliases are used.
        // "PlayerName" vs "Aliased (PlayerName)"
        // Until APCpp is forked to support this better, do it live.

        if (playerName.compare(APClient::getSlotName()) == 0)
            return true;

        std::string aliasSuffix = " (" + std::string(APClient::getSlotName()) + ")";
        if (playerName.rfind(aliasSuffix) != std::string::npos)
            return true;

        return false;
    }

    void sortHints(ImGuiTableSortSpecs *hintSortSpec)
    {
        if (hintSortSpec != nullptr) {
            std::sort(
                Hints.begin(), Hints.end(),
                [hintSortSpec](AP_HintMessage a, AP_HintMessage b)
                {
                    if (hintSortSpec->Specs->ColumnIndex == 0)
                        return a.checked > b.checked;
                    else if (hintSortSpec->Specs->ColumnIndex == 1)
                        return a.sendPlayer > b.sendPlayer;
                    else if (hintSortSpec->Specs->ColumnIndex == 2)
                        return a.recvPlayer > b.recvPlayer;
                    else if (hintSortSpec->Specs->ColumnIndex == 3)
                        return a.item > b.item;
                    else if (hintSortSpec->Specs->ColumnIndex == 4)
                        return a.location > b.location;

                    return a.item > b.item;
                }
            );
            if (hintSortSpec->Specs->SortDirection == ImGuiSortDirection_Descending)
                std::reverse(Hints.begin(), Hints.end());
        }

        resortHints = false;
    }

    void handleHintMessage(const AP_HintMessage& recvHint)
    {
        if (!isPlayer(recvHint.sendPlayer) && !isPlayer(recvHint.recvPlayer))
            return;

        for (AP_HintMessage &hint : Hints)
        {
            if (hint == recvHint)
            {
                hint.checked = recvHint.checked;
                resortHints = true;
                return;
            }
        }

        if (isPlayer(recvHint.sendPlayer))
        {
            // TODO: ID Remaps
            auto itemID = location_name_to_id[recvHint.location] / AP_ID_FACTOR;

            if (std::ranges::find(HintedIDs, itemID) == HintedIDs.end())
                HintedIDs.push_back(itemID);
        }

        Hints.push_back(recvHint);
        resortHints = true;
    }

    void refreshHints()
    {
        if (!hintsRequested) {
            auto name = "_read_hints_0_" + std::to_string(AP_GetPlayerID());
            //APClient::ServerDataRequest_Raw(name, hintsRequest, hintsRequested, hintsRaw_S);
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

        json tempHints;

        try {
            json tempHints = json::parse(hintsRaw_S);

            for (const auto& hint : tempHints) {
                if (hint["status"] != 40 || hint["item"] < 100 || hint["receiving_player"] != AP_GetPlayerID())
                    continue;

                auto itemName = item_ap_id_to_name[hint["item"]];

                for (AP_HintMessage& hint : Hints) {
                    if (hint.item.compare(itemName) == 0)
                        hint.checked = true;
                }
            }
        }
        catch (const json::parse_error& e) {
            APLogger::print(__FUNCTION__": JSON parse error: (%d) %s\n", e.byte, e.what());
        }
        catch (const std::exception& e) {
            APLogger::print(__FUNCTION__": Exception during JSON parse: %s\n", e.what());
        }

        hintsRequested = false;
        hintsRaw_S.clear();
    }

    void updateSentLocations(const std::array<int64_t, 2> &locationIDs)
    {
        // Simply no better way...
        for (const auto &locationID : locationIDs) {
            auto location = location_id_to_name[locationID];
            for (auto &hint : Hints) {
                if (isPlayer(hint.sendPlayer) && location.compare(hint.location) == 0)
                    hint.checked = true;
            }
        }

        resortHints = true;
    }

    void updateByItemName(const std::string &itemName)
    {
        if (item_name_to_ap_id[itemName] < AP_ID_FACTOR)
            return; // Without location data, good luck. Dupes make some sense at least.

        for (auto& hint : Hints) {
            if (isPlayer(hint.recvPlayer) && hint.item.compare(itemName) == 0) {
                APLogger::print(__FUNCTION__" setting checked for %s\n", hint.location.c_str());
                hint.checked = true;
            }
        }

        resortHints = true;
    }

    void ImGuiTab()
    {
        //if (!APClient::devMode) return;

        if (!init) {
            AP_Say("!hint");
            init = true;
        }

        /*if (hintsRequested)
            refreshHints();*/

        ImGui::Checkbox("Hide checked", &hintHideChecked);
        HelpMarker("Non-song items may be out of date until manually refreshed.");
        ImGui::SameLine();

        float avail = ImGui::GetContentRegionAvail().x;
        float buttonWidth = ImGui::CalcTextSize("Manual Refresh").x + ImGui::GetStyle().FramePadding.x * 2;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - buttonWidth);

        //ImGui::BeginDisabled(true);
        if (ImGui::Button("Manual Refresh")) {
            drop();
            AP_Say("!hint");
        }
        //ImGui::EndDisabled();

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Sends a !hint command in chat for better accuracy.\n"
                        "New hints are not created and hint points are not spent.");
            ImGui::EndTooltip();
        }

        ImGui::Checkbox("Show only my checks", &hintOwnLocationsOnly);

        ImGui::SameLine();
        std::string hintLabel = std::to_string(Hints.size()) + " Hints";
        avail = ImGui::GetContentRegionAvail().x;
        buttonWidth = ImGui::CalcTextSize(hintLabel.c_str()).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - buttonWidth);
        ImGui::Text("%s", hintLabel.c_str());

        if (ImGui::BeginTable("tableHints", 5,
            ImGuiTableFlags_Sortable |
            ImGuiTableFlags_BordersInner | ImGuiTableFlags_Hideable | ImGuiTableFlags_HighlightHoveredColumn |
            ImGuiTableFlags_Reorderable | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit
        ))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Checked");
            ImGui::TableSetupColumn("Finder");
            ImGui::TableSetupColumn("Receiver");
            ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_DefaultSort);
            ImGui::TableSetupColumn("Location");
            ImGui::TableHeadersRow();

            auto hintSortSpec = ImGui::TableGetSortSpecs();
            if (hintSortSpec != nullptr && (hintSortSpec->SpecsDirty || resortHints))
                sortHints(hintSortSpec);

            int uid = 0;
            for (const AP_HintMessage& hint : Hints) {
                if (hintHideChecked && hint.checked)
                    continue;

                bool isMyCheck = isPlayer(hint.sendPlayer);

                if (hintOwnLocationsOnly && !isMyCheck)
                    continue;

                // TODO: ID Remaps
                auto locID = location_name_to_id[hint.location.c_str()];
                auto itemName = item_ap_id_to_name[(locID / AP_ID_FACTOR) * AP_ID_FACTOR];

                bool haveItem = isMyCheck && std::ranges::find(recvIDs, locID / AP_ID_FACTOR) != recvIDs.end();

                ImGui::TableNextRow();

                uid += 1;
                ImGui::PushID(uid);

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", hint.checked ? "X" : " ");

                ImGui::TableNextColumn();
                ImGui::Text("%s", hint.sendPlayer.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%s", hint.recvPlayer.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%s", hint.item.c_str());

                ImGui::TableNextColumn();

                if (haveItem)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));

                ImGui::Text("%s", hint.location.c_str());

                if (haveItem)
                    ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(0);

                ImGui::Selectable("##xx", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);

                if (ImGui::BeginPopupContextItem("##xx")) {
                    if (isMyCheck) {
                        if (haveItem) {
                            ImGui::MenuItem("You have this already!", NULL, false, false);
                        }
                        else {
                            if (ImGui::MenuItem("Hint this##xx"))
                                AP_Say("!hint " + itemName);
                        }
                    }
                    else {
                        if (ImGui::MenuItem("Copy hint##xx")) {
                            std::string h = std::string(APClient::getSlotName()) + "'s " + hint.item + " is at " + hint.location + " in " + hint.sendPlayer + "'s world";
                            ImGui::SetClipboardText(h.c_str());
                        }
                    }

                    if (ImGui::MenuItem("Copy location name##xx"))
                        ImGui::SetClipboardText(hint.location.c_str());

                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }
}