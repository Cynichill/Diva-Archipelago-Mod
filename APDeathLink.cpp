#include "APDeathLink.h"
#include "Helpers.h"
#include "pch.h"
#include <filesystem>
#include <fstream>
#include <iostream>


APDeathLink::APDeathLink()
{
}

void APDeathLink::config(toml::v3::ex::parse_result& data)
{
    std::string config_percent = data["deathlink_percent"].value_or(std::to_string(percent));
    percent = std::clamp(std::stoi(config_percent), 0, 100);

    std::cout << "[Archipelago] deathlink_percent set to " << percent << " (config: " << config_percent << ")" << std::endl;

    std::string config_safety = data["deathlink_safety"].value_or(std::to_string(safety));
    safety = std::clamp(std::stof(config_safety), 0.0f, 30.0f);

    std::cout << "[Archipelago] deathlink_safety set to " << safety << " (config: " << config_safety << ")" << std::endl;

    reset();
}

bool APDeathLink::exists()
{
	return std::filesystem::exists(DeathLinkInFile);
}

int APDeathLink::touch()
{
    deathLinked = true;

	if (!exists()) {
        std::ofstream death_link_out(DeathLinkOutFile);
        
        if (!death_link_out.is_open()) {
            std::cout << "[Archipelago] DeathLink > Failed to send death_link_out" << std::endl;
            return 1;
        }

        death_link_out.close();
        std::cout << "[Archipelago] DeathLink > Sending death_link_out" << std::endl;
	}

	return 0;
}

void APDeathLink::reset()
{
    std::cout << "[Archipelago] DeathLink: deathLinked = " << deathLinked << " -> " << false << std::endl;

    deathLinked = false;
    remove(DeathLinkInFile.c_str());
    remove(DeathLinkOutFile.c_str());
}

void APDeathLink::fail()
{
    if (deathLinked) {
        std::cout << "[Archipelago] DeathLink > Fail: Currently dying" << std::endl;
        return;
    }

    auto deltaLast = *(float*)DivaGameTimer - lastDeathLink;

    if (deltaLast < safety) {
        std::cout << "[Archipelago] DeathLink > Fail: Died in safety window" << std::endl;
        return;
    }

    touch();
}

void APDeathLink::run()
{
    if (!exists() || deathLinked)
        return;

    std::cout << "[Archipelago] DeathLink < death_link_in" << std::endl;

    lastDeathLink = *(float*)DivaGameTimer;
    uint8_t currentHP = (uint8_t)DivaGameHP;

    int hit = (255 * percent) / 100 + 1;
    currentHP = std::clamp(currentHP - hit, 0, 255);
    deathLinked = (currentHP > 0) ? false : true;

    WRITE_MEMORY(DivaGameHP, uint8_t, static_cast<uint8_t>(currentHP));

    remove(DeathLinkInFile.c_str());
}
