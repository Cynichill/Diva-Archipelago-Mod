#pragma once
#include "pch.h"

namespace APClient
{
    extern bool devMode;

    extern int clearGrade; // -> main
    extern std::vector<int> CheckedLocations; // -> IDHandler
    extern std::vector<int> seedIDs; // -> IDHandler
    extern std::vector<int> recvIDs; // Song IDs received as items (not item IDs)
    extern std::vector<int> missingIDs; // -> IDHandler
    extern int victoryID; // -> IDHandler
    extern std::unordered_map<std::string, uint32_t> item_name_to_ap_id;
    extern std::unordered_map<uint32_t, std::string> item_ap_id_to_name; // -> IDHandler
    extern std::unordered_map<std::string, uint32_t> location_name_to_id;
    extern std::unordered_map<uint32_t, std::string> location_id_to_name;

    extern int leekHave;
    extern int leekNeed;

    void UpdateMissing();

    char* getSlotName();

    void config(toml::v3::ex::parse_result& data);
    void reset();

    AP_RequestStatus ServerDataRequest_Raw(std::string key, AP_GetServerDataRequest& request, bool& requested, std::string& output);

    void GetSlotData();

    void ItemClear();
    void ItemRecv(int64_t, bool);
    void PushRecvID(int);
    void LocationChecked(int64_t);
    void LocationSend(int64_t pvID);

    void LogAppend(const std::string& text);
    void CheckMessages();

    void RecvDeath(std::string, std::string);
    void SendDeath();


    bool LoadDatapackage();
    void ImGuiTab();
}

