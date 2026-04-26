#include "APClient.h"
#include "APDeathLink.h"

namespace APDeathLink
{
    bool &devMode = APClient::devMode;

    // Config options
    bool death_link = false; // In-game state, not APCpp. Connection should always have the DeathLink tag from APCpp.
    int death_link_amnesty = 0; // Pair with death_link_amnesty_count
    int death_link_percent = 100; // Percentage of max HP to lose on receive. "If at or below this, die."
    float death_link_safety = 10.0f; // Seconds after receiving a DL to avoid chain reaction DLs.

    const uint64_t DivaGameHP = 0x1412C2330 + 0x2D234;
    const uint64_t DivaGameTimer = 0x1412C2330 + 0x2C010;
    const uint64_t DivaSafetyWidthPercent = 0x1412C2330 + 0x2D314;

    // Internal
    int death_link_amnesty_count = 0;
    bool deathLinked = false; // Set after calling a kill so future kills are ignored (until reset)

    float lastDeathLink = 0.0f; // Compared against APDeathLink::death_link_safety
    float lastCheckedHP = 0.0f; // HP: For delta time against APDeathLink::DivaGameTimer

    // Progressive HP
    int HPreceived = 1; // Current HP chunks received
    int HPtemp = 0; // Temporarily added chunks
    int HPnumerator = 1; // Received and potentially temp extras
    int HPdenominator = 1; // Total HP chunks
    int HPprefloor = 76; // 0-255, default safety HP. used for rolling safety bar.
    int HPprepercent = 30; // 0-100, default safety bar percentage. used for rolling safety bar.
    int HPfloor = 0; // 0-255, highest HP value that triggers death and when to engage
    int HPpercent = 0; // 0-100, percentage of safety bar
    bool HPengaged = false; // True when HPfloor is met
    bool safetyExpired = false; // Easy 60, Norm 40, Hard+ 30s

    void config(const toml::table& settings)
    {
        toml::table section;
        if (settings.contains("death_link") && settings["death_link"].is_table())
            section = *settings["death_link"].as_table();

        death_link = settings["death_link_enabled"].value_or(false);
        APLogger::print("death_link_enabled set to %d (config: %d)\n", death_link);

        int config_death_link_amnesty = settings["death_link_amnesty"].value_or(0);
        death_link_amnesty = std::clamp(config_death_link_amnesty, 0, 20);
        death_link_amnesty_count = death_link_amnesty;
        APLogger::print("death_link_amnesty set to %d (config: %d)\n", death_link_amnesty, config_death_link_amnesty);

        int config_percent = settings["death_link_percent"].value_or(death_link_percent);
        death_link_percent = std::clamp(config_percent, 0, 100);
        APLogger::print("death_link_percent set to %d (config: %d)\n", death_link_percent, config_percent);

        float config_safety = settings["death_link_safety"].value_or(death_link_safety);
        death_link_safety = std::clamp(config_safety, 0.0f, 30.0f);
        APLogger::print("death_link_safety set to %.02f (config: %.02f)\n", death_link_safety, config_safety);

        reset();
    }

    void save(toml::table& settings)
    {
        toml::table config;
        config.insert("death_link_enabled", death_link);
        config.insert("death_link_amnesty", death_link_amnesty);
        config.insert("death_link_percent", death_link_percent);
        config.insert("death_link_safety", death_link_safety);

        settings.insert("death_link", config);
    }

    void reset()
    {
        APLogger::print("DeathLink: reset\n");

        deathLinked = false;
        lastDeathLink = 0.0f;
        lastCheckedHP = 0.0f;

        prog_hp_reset();
    }

    void runAmnesty()
    {
        if (!death_link) return;

        if (deathLinked) return; // Death already handled.

        // TODO: Slot aliases?
        static std::string msg = "The Disappearance of " + std::string(APClient::getSlotName());

        if (death_link_amnesty == 0 || death_link_amnesty_count == 0) {
            death_link_amnesty_count = death_link_amnesty;
            AP_DeathLinkSend(msg);
            return;
        }

        death_link_amnesty_count -= 1;
    }


    void prog_hp_update()
    {
        bool changed = false;
        static int prevHP = HPnumerator;

        HPnumerator = HPreceived + HPtemp;
        HPnumerator = min(HPnumerator, HPdenominator);

        if (HPnumerator != prevHP)
        {
            prevHP = HPnumerator;
            changed = true;
        }

        if (HPnumerator >= HPdenominator) {
            prog_hp_reset();
            return;
        }

        // Get portion of HP
        int available = (255 * HPnumerator) / HPdenominator;
        available = std::clamp((int)available, 1, 255);

        HPfloor = 255 - available;
        HPpercent = (HPfloor * 100) / 255 - 1;

        if (changed)
            APLogger::print("[%6.2f] DeathLinkHP < %i / %i = %i%% (%i HP)\n",
                lastCheckedHP, HPnumerator, HPdenominator, HPpercent, HPfloor);

        if (!HPengaged) {
            // Roll 6% behind current HP. One day find the HP bar % address.
            int hp_percent = (*(uint8_t*)DivaGameHP * 100) / 255;

            if (HPprepercent < hp_percent - 6) {
                HPprepercent += 1;
                HPprefloor = (255 * HPprepercent) / 100;
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

        /*HPnumerator = 1;
        HPdenominator = 1;*/
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

        if (deltaLast < death_link_safety) {
            deathLinked = true;
            APLogger::print("[%6.2f] DeathLink > Fail: Died in safety window (%.02f + %.02f < %.02f)\n",
                now, lastDeathLink, deltaLast, lastDeathLink + death_link_safety);
            return;
        }

        runAmnesty();
        deathLinked = true;
    }

    void run(bool received)
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
                setHP(0);
            }

            if (safetyExpired && HPengaged && currentHP <= HPfloor) {
                APLogger::print("[%6.2f] DeathLinkHP > Tripped at %i HP\n", now, currentHP);
                setHP(0);
            }
            else if (!HPengaged && currentHP >= HPfloor + 3) {
                APLogger::print("[%6.2f] DeathLinkHP < Engaged at %i HP\n", now, currentHP);
                HPengaged = true;
                prog_hp_update();
            }
        }

        if (deathLinked || !received)
            return;

        lastDeathLink = now;

        int hit = (255 - HPfloor) * death_link_percent / 100;
        if (death_link_percent == 50)
            hit += 1;

        int toHP = std::clamp(currentHP - hit, 0, 255);
        if (death_link_percent == 100)
            toHP = 0; // for prog HP and other exceptions. 100% is DEAD.

        deathLinked = (toHP > 0) ? false : true;

        APLogger::print("[%6.2f] DeathLink < death_link_in (%i - %i = %i / DL: %i)\n",
            now, currentHP, hit, toHP, deathLinked);

        currentHP = toHP;
        setHP(currentHP);
    }

    void setHP(uint8_t HP)
    {
        WRITE_MEMORY(DivaGameHP, int, static_cast<uint8_t>(HP));
    }

    void ImGuiTab()
    {
        if (ImGui::BeginTabItem("Death Link")) {
            if (devMode || HPdenominator > 1) {
                float progress = (float)min(HPdenominator, (HPdenominator - HPnumerator)) / (float)HPdenominator;
                char buf[8];
                sprintf(buf, "%d / %d", HPnumerator, HPdenominator);

                ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f), buf);
                ImGui::SameLine();
                ImGui::Text("Progressive HP");

                if (ImGui::Button("Reset##progReset"))
                    HPtemp = 0;

                ImGui::SameLine();
                if (ImGui::Button("+1##progTemp+1"))
                    HPtemp += 1;


                ImGui::SameLine();
                ImGui::Text("Temporary HP: %d+%d", HPreceived, HPtemp);

                ImGui::SameLine();
                HelpMarker("Temporarily increase available chunk count.\nResets when the next one is received.");

                if (devMode)
                    ImGui::SliderInt("Denominator", &HPdenominator, 1, 20);

                ImGui::Separator();
            }

            ImGui::Checkbox("Death Link", &death_link);
            ImGui::SameLine();
            HelpMarker("When you die on your own or fail to reach Grade Needed (not both), everyone with Death Link enabled dies.");

            if (death_link) {
                if (ImGui::SliderInt("Death Link Amnesty", &death_link_amnesty, 0, 20))
                    death_link_amnesty_count = death_link_amnesty;
                ImGui::SameLine();
                HelpMarker("Amount of additional own deaths needed before sending one Death Link. 0 would be every death, 1 every other, etc.");

                if (death_link_amnesty > 0) {
                    char overlay[64];
                    sprintf(overlay, "%d / %d deaths", death_link_amnesty - death_link_amnesty_count, death_link_amnesty);
                    ImGui::ProgressBar(static_cast<float>(death_link_amnesty - death_link_amnesty_count) / static_cast<float>(death_link_amnesty), ImVec2(0,0), overlay);
                }

                ImGui::SliderInt("Death Link Percent", &death_link_percent, 0, 100, "%d%%");
                ImGui::SameLine();
                HelpMarker("Percent of max HP to lose on receive.\n<100 for non-lethal, but makes Life Bonuses harder which may affect score by up to 2%.");

                ImGui::SliderFloat("Death Link Safety", &death_link_safety, 0.0f, 30.0f, "%.1f seconds");
                ImGui::SameLine();
                HelpMarker("Seconds after receiving where dying does not send one out.");

                if (devMode)
                {
                    ImGui::Separator();

                    if (ImGui::Button("100%")) {
                        deathLinked = true;
                        setHP(0);
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("+NoFail##100")) {
                        deathLinked = true;
                        WRITE_MEMORY(0x1412C2330 + 0x2D31D, bool, 0);
                        setHP(0);
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Recv"))
                        run(true);

                    ImGui::SameLine();
                    if (ImGui::Button("+NoFail##recv")) {
                        WRITE_MEMORY(0x1412C2330 + 0x2D31D, bool, 0);
                        run(true);
                    }

                    ImGui::SameLine();
                    ImGui::Text("Linked: %d", deathLinked);
                    ImGui::SameLine();
                    HelpMarker("If 1/true, the cause of the death prevented a Death Link from being sent.\nFor example, dying in one hit or inside the safety window.");
                }
            }

            ImGui::EndTabItem();
        }
    }
}
