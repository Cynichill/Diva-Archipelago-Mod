#pragma warning( disable : 4244 )
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
    int config_percent = data["deathlink_percent"].value_or(percent);
    percent = std::clamp(config_percent, 0, 100);

    APLogger::print("deathlink_percent set to %d (config: %d)\n", percent, config_percent);

    float config_safety = data["deathlink_safety"].value_or(safety);
    safety = std::clamp(config_safety, 0.0f, 30.0f);

    APLogger::print("deathlink_safety set to %.02f (config: %.02f)\n", safety, config_safety);

    fs::remove(LocalPath / DeathLinkInFile);
    fs::remove(LocalPath / DeathLinkOutFile);

    reset();
}

bool APDeathLink::exists(const fs::path& in)
{
	return fs::exists(LocalPath / in);
}

int APDeathLink::touch()
{
    deathLinked = true;

	if (!exists(DeathLinkInFile)) {
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
    APLogger::print("DeathLink: reset\n");

    deathLinked = false;
    lastDeathLink = 0.0f;
    lastCheckedHP = 0.0f;

    prog_hp_reset();
}

void APDeathLink::prog_hp_update()
{
    if (!exists(HPFile)) {
        prog_hp_reset();
        return;
    }

    bool changed = false;
    std::ifstream file(LocalPath / HPFile);

    if (file.is_open()) {
        int i = 0;

        while (std::getline(file, buf)) {
            if (i >= 2)
                break;

            try {
                auto val = std::clamp(std::stoi(buf), 1, 255);

                if (i == 0 && val != HPnumerator) {
                    changed = true;
                    HPnumerator = val;
                }
                if (i == 1 && val != HPdenominator) {
                    changed = true;
                    HPdenominator = val;
                }
            }
            catch (std::invalid_argument const& ex) {
                APLogger::print("DeathLinkHP > %s\n", ex.what());
            }
            catch (std::out_of_range const& ex) {
                APLogger::print("DeathLinkHP > %s\n", ex.what());
            }

            i += 1;
        }

        if (HPnumerator > HPdenominator)
            HPnumerator = HPdenominator;
    }

    if (changed) {
        if (HPdenominator == HPnumerator) {
            prog_hp_reset();
            return;
        }

        // Get portion of HP
        int available = (255.0f / (float)HPdenominator) * HPnumerator;
        available = std::clamp((int)available + 1, 1, 255);

        HPfloor = 255 - available;
        HPpercent = 1 + ((float)HPfloor / 255.0f) * 100.0f;

        APLogger::print("[%6.2f] DeathLinkHP < %i / %i = %i%% (%i HP)\n",
                        lastCheckedHP, HPnumerator, HPdenominator, HPpercent, HPfloor);
    }

    if (!HPengaged) {
        // Roll 6% behind current HP. One day find the HP bar % address.
        int hp_percent = ((float)*(uint8_t*)DivaGameHP / 255.0f) * 100.0f;

        if (HPprepercent < hp_percent - 6) {
            HPprepercent += 1;
            HPprefloor = 255.0f * ((float)HPprepercent / 100.0f);
            WRITE_MEMORY(DivaSafetyWidthPercent, int, static_cast<uint8_t>(HPprepercent));
        }
    }
    else {
        if (HPpercent > 0)
            WRITE_MEMORY(DivaSafetyWidthPercent, int, static_cast<uint8_t>(HPpercent));
    }
}

void APDeathLink::prog_hp_reset()
{
    if (HPdenominator == 1)
        return;

    HPnumerator = 1;
    HPdenominator = 1;
    HPprefloor = 76;
    HPprepercent = 30;
    HPfloor = 0;
    HPpercent = 0;
    HPengaged = false;

    WRITE_MEMORY(DivaSafetyWidthPercent, int, 30);
}

void APDeathLink::check_fail()
{
    if (*(uint8_t*)DivaGameHP > 0)
        return;

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
        APLogger::print("[%6.2f] DeathLink > Fail: Died in safety window (%.02f + %.02f < %.02f)\n",
                        now, lastDeathLink, deltaLast, lastDeathLink + safety);
        return;
    }

    touch();
}

void APDeathLink::run()
{
    auto now = *(float*)DivaGameTimer;

    // Avoid stopping the fade in from white animation at the start of a song and
    // prevents No Fail -> DL to 0 HP -> Return to song select instead of results -> Play
    if (now == 0.0f) {
        reset();
        return;
    }

    int currentHP = *(uint8_t*)DivaGameHP;

    if (now - lastCheckedHP > 1.0f) {
        lastCheckedHP = now;
        prog_hp_update();
    }

    if (HPpercent > 0) {
        if (safetyExpired && !HPengaged && currentHP <= HPprefloor) {
            // Default safety window expired. Use rolling safety as kill floor.
            APLogger::print("[%6.2f] DeathLinkHP > Tripped at %i HP (rolling)\n", now, currentHP);
            WRITE_MEMORY(DivaGameHP, int, 0);
        }

        if (safetyExpired && HPengaged && currentHP <= HPfloor) {
            APLogger::print("[%6.2f] DeathLinkHP > Tripped at %i HP\n", now, currentHP);
            WRITE_MEMORY(DivaGameHP, int, 0);
        }
        else if (!HPengaged && currentHP >= HPfloor + 3) {
            APLogger::print("[%6.2f] DeathLinkHP < Engaged at %i HP\n", now, currentHP);
            HPengaged = true;
            prog_hp_update();
        }
    }

    if (deathLinked || !exists(DeathLinkInFile))
        return;

    lastDeathLink = now;

    int hit = (255 - HPfloor) * percent / 100;
    if (percent == 50)
        hit += 1;

    int toHP = std::clamp(currentHP - hit, 0, 255);
    deathLinked = (toHP > 0) ? false : true;

    APLogger::print("[%6.2f] DeathLink < death_link_in (%i - %i = %i / DL: %i)\n",
                    now, currentHP, hit, toHP, deathLinked);

    currentHP = toHP;
    WRITE_MEMORY(DivaGameHP, int, static_cast<uint8_t>(currentHP));

    fs::remove(LocalPath / DeathLinkInFile);
}
