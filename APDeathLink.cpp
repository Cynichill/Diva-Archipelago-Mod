#pragma warning( disable : 4244 )
#include "APDeathLink.h"

namespace APDeathLink
{
    // Config options
    int percent = 100; // Percentage of max HP to lose on receive. "If at or below this, die."
    float safety = 10.0f; // Seconds after receiving a DL to avoid chain reaction DLs.

    // Com
    extern const fs::path LocalPath = fs::current_path();
    const fs::path DeathLinkInFile = "death_link_in"; // Inbound communication file
    const fs::path DeathLinkOutFile = "death_link_out"; // Outbound communication file
    const fs::path HPFile = "hp.txt"; // Progressive HP file with ext for easy editing

    const uint64_t DivaGameHP = 0x00000001412EF564;
    const uint64_t DivaGameTimer = 0x00000001412EE340;
    const uint64_t DivaSafetyWidthPercent = 0x00000001412EF644;

    // Internal
    bool deathLinked = false; // Who wants to know?
    float lastDeathLink = 0.0f; // Compared against APDeathLink::safety
    float lastCheckedHP = 0.0f; // HP: For delta time against APDeathLink::DivaGameTimer
    int HPnumerator = 1; // Current HP chunks received
    int HPdenominator = 1; // Total HP chunks
    int HPprefloor = 76; // 0-255, default safety HP. used for rolling safety bar.
    int HPprepercent = 30; // 0-100, default safety bar percentage. used for rolling safety bar.
    int HPfloor = 0; // 0-255, highest HP value that triggers death and when to engage
    int HPpercent = 0; // 0-100, percentage of safety bar
    bool HPengaged = false; // True when HPfloor is met
    bool safetyExpired = false; // Easy 60, Norm 40, Hard+ 30s
    std::string buf;

    void config(toml::v3::ex::parse_result& data)
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

    bool exists(const fs::path& in)
    {
        return fs::exists(LocalPath / in);
    }

    int touch()
    {
        deathLinked = true;

        if (!APDeathLink::exists(DeathLinkInFile)) {
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

    void reset()
    {
        APLogger::print("DeathLink: reset\n");

        deathLinked = false;
        lastDeathLink = 0.0f;
        lastCheckedHP = 0.0f;

        prog_hp_reset();
    }

    void prog_hp_update()
    {
        fs::path hp_path = LocalPath / HPFile;

        std::ifstream file;
        file.open(hp_path);

        // TODO: remove the non-.txt fallback after reasonable migration time.
        if (!file.is_open()) {
            hp_path.replace_extension("");
            file.open(hp_path);
        }

        if (!file.is_open()) {
            prog_hp_reset();
            return;
        }

        bool changed = false;

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

        HPnumerator = min(HPnumerator, HPdenominator);

        if (changed) {
            if (HPdenominator == HPnumerator) {
                prog_hp_reset();
                return;
            }

            // Get portion of HP
            int available = (255.0f / (float)HPdenominator) * HPnumerator;
            available = std::clamp((int)available, 1, 255);

            HPfloor = 255 - available;
            HPpercent = ((float)HPfloor / 255.0f) * 100.0f - 1;

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

    void prog_hp_reset()
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

    void check_fail()
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

    void run()
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

        if (deathLinked || !APDeathLink::exists(DeathLinkInFile))
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
}
