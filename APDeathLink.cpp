#include "APDeathLink.h"
#include "APLogger.h"
#include "Helpers.h"
#include "pch.h"
#include <fstream>
#include <iostream>


APDeathLink::APDeathLink()
{
}

void APDeathLink::config(toml::v3::ex::parse_result& data)
{
    std::string config_percent = data["deathlink_percent"].value_or(std::to_string(percent));
    percent = std::clamp(std::stoi(config_percent), 0, 100);

    APLogger::print("deathlink_percent set to %i (config: %s)\n", percent, config_percent);

    std::string config_safety = data["deathlink_safety"].value_or(std::to_string(safety));
    safety = std::clamp(std::stof(config_safety), 0.0f, 30.0f);

    APLogger::print("deathlink_safety set to %.02f (config: %s)\n", safety, config_safety);

    reset();
}

bool APDeathLink::exists()
{
	return fs::exists(LocalPath / DeathLinkInFile);
}

int APDeathLink::touch()
{
    deathLinked = true;

	if (!exists()) {
        std::ofstream death_link_out(LocalPath / DeathLinkOutFile);

        if (!death_link_out.is_open()) {
            APLogger::print("DeathLink > Failed to send death_link_out\n");
            return 1;
        }

        death_link_out.close();
        APLogger::print("DeathLink > Sending death_link_out\n");
	}

	return 0;
}

void APDeathLink::reset()
{
    APLogger::print("DeathLink: reset\n", deathLinked);

    deathLinked = false;
    lastDeathLink = 0;
    fs::remove(LocalPath / DeathLinkInFile);
    fs::remove(LocalPath / DeathLinkOutFile);
}

void APDeathLink::fail()
{
    if (deathLinked) {
        APLogger::print("DeathLink > Fail: Already dying\n");
        return;
    }

    auto now = *(float*)DivaGameTimer;
    if (lastDeathLink > now)
        lastDeathLink = now;

    auto deltaLast = now - lastDeathLink;

    if (deltaLast < safety) {
        deathLinked = true;
        APLogger::print("DeathLink > Fail: Died in safety window @ %.02f + %.02f < %.02f\n", lastDeathLink, deltaLast, lastDeathLink + safety);
        return;
    }

    touch();
}

void APDeathLink::run()
{
    auto now = *(float*)DivaGameTimer;

    // Avoid stopping the fade in from white animation at the start of a song.
    if (now == 0.0f) {
        if (lastDeathLink != 0)
            reset();
        return;
    }

    int currentHP = *(uint8_t*)DivaGameHP;

    // Exception for No Fail -> DL to 0 HP -> Return to song select instead of results -> Play
    if (currentHP > 0 && deathLinked)
        reset();

    if (deathLinked || !exists())
        return;

    APLogger::print("DeathLink < death_link_in @ %.02f\n", now);

    lastDeathLink = now;

    int hit = (255 * percent) / 100 + 1;
    currentHP = std::clamp(currentHP - hit, 0, 255);
    deathLinked = (currentHP > 0) ? false : true;

    WRITE_MEMORY(DivaGameHP, int, static_cast<uint8_t>(currentHP));

    fs::remove(LocalPath / DeathLinkInFile);
}
