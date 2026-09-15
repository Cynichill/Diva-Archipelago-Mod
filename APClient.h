#pragma once
#include "pch.h"

namespace APClient
{
    extern bool devMode;

    // AP supports item and location IDs up to int64_t (0 and negatives reserved)
    // The AP impl for Diva packs in songs IDs to (songID*10) and locations to (songID*10), (songID*10)+1
    // This may change in the future to *100. Project Diva currently seems limited to int32, so int64 it is.
    extern std::vector<int64_t> CheckedLocations;
    extern std::vector<int64_t> seedIDs; // From slot data, the Song IDs (not item IDs) in the seed
    extern std::vector<int64_t> recvIDs; // Song IDs received as items (not item IDs)
    extern std::vector<int64_t> missingIDs; // The difference between seedIDs and recvIDs
    extern std::unordered_map<std::string, int64_t> item_name_to_ap_id;
    extern std::unordered_map<int64_t, std::string> item_ap_id_to_name;
    extern std::unordered_map<std::string, int64_t> location_name_to_id;
    extern std::unordered_map<int64_t, std::string> location_id_to_name;

    // Slot and play data

    extern int64_t victoryID;
    extern int clearGrade;
    extern int leekHave;
    extern int leekNeed;
    extern int locHave;
    extern int locNeed;

    void UpdateMissing();

    char* getSlotName();

    void config(const toml::table& settings);
    void save(toml::table& settings);
    void reset();

    void PushRecvID(int64_t songID);
    void LocationSend(int64_t pvID);

    void LogAppend(const std::string& text);
    void DataRequest(const std::string key, const std::function<void(std::string raw)> callback);
    void CheckMessages();

    void RecvDeath(const std::string& src, const std::string& cause);

    bool LoadDatapackage();

    void UpdateTags();

    void ImGuiTab();
}

