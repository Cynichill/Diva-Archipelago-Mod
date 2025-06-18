#ifndef VIRTUALKEY_H
#define VIRTUALKEY_H

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <toml++/toml.h>
#include <windows.h>
#include "deck.h"


// Function to convert key name to virtual key code
u8 GetVirtualKeyCode(const std::string& keyName) {
    static const std::unordered_map<std::string, u8> keyMap = {
        {"BACKSPACE", VK_BACK}, {"TAB", VK_TAB}, {"CLEAR", VK_CLEAR},
        {"ENTER", VK_RETURN}, {"SHIFT", VK_SHIFT}, {"CTRL", VK_CONTROL},
        {"ALT", VK_MENU}, {"PAUSE", VK_PAUSE}, {"CAPSLOCK", VK_CAPITAL},
        {"ESCAPE", VK_ESCAPE}, {"SPACE", VK_SPACE}, {"PAGEUP", VK_PRIOR},
        {"PAGEDOWN", VK_NEXT}, {"END", VK_END}, {"HOME", VK_HOME},
        {"LEFT", VK_LEFT}, {"UP", VK_UP}, {"RIGHT", VK_RIGHT},
        {"DOWN", VK_DOWN}, {"SELECT", VK_SELECT}, {"PRINT", VK_PRINT},
        {"EXECUTE", VK_EXECUTE}, {"PRNTSCRN", VK_SNAPSHOT}, {"INS", VK_INSERT},
        {"DEL", VK_DELETE}, {"HELP", VK_HELP}, {"LWIN", VK_LWIN},
        {"RWIN", VK_RWIN}, {"APPS", VK_APPS}, {"NUMPAD0", VK_NUMPAD0},
        {"NUMPAD1", VK_NUMPAD1}, {"NUMPAD2", VK_NUMPAD2}, {"NUMPAD3", VK_NUMPAD3},
        {"NUMPAD4", VK_NUMPAD4}, {"NUMPAD5", VK_NUMPAD5}, {"NUMPAD6", VK_NUMPAD6},
        {"NUMPAD7", VK_NUMPAD7}, {"NUMPAD8", VK_NUMPAD8}, {"NUMPAD9", VK_NUMPAD9},
        {"MULTIPLY", VK_MULTIPLY}, {"ADD", VK_ADD}, {"SEPARATOR", VK_SEPARATOR},
        {"SUBTRACT", VK_SUBTRACT}, {"DECIMAL", VK_DECIMAL}, {"DIVIDE", VK_DIVIDE},
        {"F1", VK_F1}, {"F2", VK_F2}, {"F3", VK_F3}, {"F4", VK_F4},
        {"F5", VK_F5}, {"F6", VK_F6}, {"F7", VK_F7}, {"F8", VK_F8},
        {"F9", VK_F9}, {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12},
        {"NUMLOCK", VK_NUMLOCK}, {"SCROLL", VK_SCROLL}, {"LSHIFT", VK_LSHIFT},
        {"RSHIFT", VK_RSHIFT}, {"LCONTROL", VK_LCONTROL}, {"RCONTROL", VK_RCONTROL},
        {"LMENU", VK_LMENU}, {"RMENU", VK_RMENU}, {"0", '0'}, {"1", '1'},
        {"2", '2'}, {"3", '3'}, {"4", '4'}, {"5", '5'}, {"6", '6'},
        {"7", '7'}, {"8", '8'}, {"9", '9'}, {"A", 'A'}, {"B", 'B'},
        {"C", 'C'}, {"D", 'D'}, {"E", 'E'}, {"F", 'F'}, {"G", 'G'},
        {"H", 'H'}, {"I", 'I'}, {"J", 'J'}, {"K", 'K'}, {"L", 'L'},
        {"M", 'M'}, {"N", 'N'}, {"O", 'O'}, {"P", 'P'}, {"Q", 'Q'},
        {"R", 'R'}, {"S", 'S'}, {"T", 'T'}, {"U", 'U'}, {"V", 'V'},
        {"W", 'W'}, {"X", 'X'}, {"Y", 'Y'}, {"Z", 'Z'}
    };

    auto it = keyMap.find(keyName);
    if (it != keyMap.end()) {
        return it->second;
    }
    else {
        std::cerr << "Unknown key: " << keyName << std::endl;
        return 0; // Return a default value or handle the error
    }
}

// Function to read the key code from the config file
u8 GetReloadKeyCode(const std::string& key) {

    try {
        if (key.empty()) {
            throw std::invalid_argument("Empty key provided");
        }

        return GetVirtualKeyCode(key); // Return the reload value
    }
    catch (const std::exception& e) {
        std::cerr << "Error retrieving virtual key code: " << e.what() << std::endl;
        return VK_F7; // Default value if an exception occurs
    }
}

#endif // KEYMAPPING_H
