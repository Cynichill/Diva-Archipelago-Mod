#pragma once
#include "pch.h"

enum class APHintStatus : int {
    HINT_UNSPECIFIED = 0,
    HINT_NO_PRIORITY = 10,
    HINT_AVOID = 20,
    HINT_PRIORITY = 30,
    HINT_FOUND = 40, // The location has been collected.Status cannot be changed once found.
};

struct APHint {
    int receive_player;
    int finding_player;
    int location;
    bool found; // checked
    std::string entrance;
    int item_flags;
    APHintStatus status;
};

namespace APHints
{
    extern std::vector<int64_t> HintedIDs;

    void reset();
    void drop();
    bool isPlayer(const std::string &playerName);
    void handleHintMessage(const AP_HintMessage&);
    void updateSentLocations(const std::array<int64_t, 2> &locationIDs);
    void updateByItemName(const std::string &itemName);

    void ImGuiTab();
}
