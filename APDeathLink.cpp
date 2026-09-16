#include "APClient.h"
#include "APDeathLink.h"
#include "APGUI.h"

namespace APDeathLink
{
    bool &devMode = APClient::devMode;

    // Config options
    bool death_link = false; // In-game state, not APCpp. Connection should always have the DeathLink tag from APCpp.
    bool death_link_self = false; // Specifically for co-op play, if slot can kill itself.
    int death_link_amnesty = 0; // Pair with death_link_amnesty_count
    int death_link_percent = 100; // Percentage of max HP to lose on receive. "If at or below this, die."
    float death_link_safety = 10.0f; // Seconds after receiving a DL to avoid chain reaction DLs.
    bool auto_retry = false; // True: queue a song reset if a DL would kill

    std::vector<std::string> death_link_tags = { "DeathLink" }; // Potential for DL Groups

    const uint64_t DivaGameHP = PvPlayData + 0x2D234;
    const uint64_t DivaGameTimer = PvPlayData + 0x2C010;
    const uint64_t DivaSafetyWidthPercent = PvPlayData + 0x2D314;

    // Internal
    int death_link_amnesty_count = 0;
    bool deathLinked = false; // true after calling a kill so future kills are ignored (until reset)

    /*
    True: the next time Death Link checks run, reset the song: resetSong()
    Test cases: dying at 0 HP (hook), dying to prog HP, dying to a DL.
    */
    bool resetQueued = false;

    void* _PvReset = sigScan("\x48\x89\x5c\x24\x10\x48\x89\x74\x24\x18\x55\x57\x41\x54\x41\x56\x41\x57\x48\x8b\xec\x48\x81\xec\x80\x00\x00\x00\x0f\x29\x74\x24\x70\x48\x8b\x05\x50\x8c\xb5\x00\x48\x33\xc4\x48\x89\x45\xe0\x48\x8b\xf9",
                            "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    // A very hookable function (NC, ModifiersAPI, ...) but we just want to call it.
    // Avoid calling directly as a result of networking (bounce handling): easy crashes!
    // Set resetQueued instead to call the next time Death Link runs.
    auto PvReset = reinterpret_cast<void(__fastcall*)(uint64_t)>(_PvReset);

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

        death_link = section["enabled"].value_or(false);
        APLogger::print("death_link enabled set to %i\n", death_link);

        death_link_amnesty = std::clamp(section["amnesty"].value_or(death_link_amnesty), 0, 20);
        death_link_amnesty_count = death_link_amnesty;
        APLogger::print("death_link amnesty set to %i\n", death_link_amnesty);

        death_link_percent = std::clamp(section["percent"].value_or(death_link_percent), 0, 100);
        APLogger::print("death_link percent set to %i\n", death_link_percent);

        death_link_safety = std::clamp(section["safety"].value_or(death_link_safety), 0.0f, 30.0f);
        APLogger::print("death_link safety set to %.02f\n", death_link_safety);

        death_link_self = section["kill_self"].value_or(false);
        APLogger::print("death_link kill_self set to %i\n", death_link_self);

        auto_retry = section["auto_retry"].value_or(auto_retry);
        APLogger::print("death_link auto_retry set to %i\n", auto_retry);

        reset();
    }

    void save(toml::table& settings)
    {
        toml::table config;
        config.insert("enabled", death_link);
        config.insert("amnesty", death_link_amnesty);
        config.insert("percent", death_link_percent);
        config.insert("safety", death_link_safety);
        config.insert("kill_self", death_link_self);
        config.insert("auto_retry", auto_retry);

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
        if (!death_link || deathLinked) return;

        if (death_link_amnesty != 0 && death_link_amnesty_count != 0) {
            death_link_amnesty_count -= 1;
            APLogger::print("DeathLink > Amnesty: %i / %i\n", death_link_amnesty - death_link_amnesty_count, death_link_amnesty);
            return;
        }

        APLogger::print("DeathLink > Send\n");
        death_link_amnesty_count = death_link_amnesty;

        AP_Bounce bounce;
        bounce.tags = &death_link_tags;

        json data;
        data["time"] = (int64_t)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        data["source"] = APClient::getSlotName();
        data["cause"] = std::format("The Disappearance of {}", APClient::getSlotName()); // TODO: Slot aliases?
        bounce.data = data.dump();

        AP_SendBounce(bounce);
    }

    void recvHP()
    {
        HPtemp = 0;
        HPreceived += 1;
        HPnumerator = HPreceived;
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
        available = std::clamp(available, 1, 255);

        HPfloor = 255 - available;
        HPpercent = (HPfloor * 100) / 255 - 1;

        if (changed)
            APLogger::print("[%6.2f] DeathLinkHP < %i / %i = %i%% (%i HP)\n",
                lastCheckedHP, HPnumerator, HPdenominator, HPpercent, HPfloor);

        if (!HPengaged) {
            // Roll 6% behind current HP. One day find the HP bar % address.
            int hp_percent = (*(int*)DivaGameHP * 100) / 255;

            if (HPprepercent < hp_percent - 6) {
                HPprepercent += 1;
                HPprefloor = (255 * HPprepercent) / 100;
                WRITE_MEMORY(DivaSafetyWidthPercent, int, HPprepercent);
            }
        }
        else {
            if (HPpercent > 0)
                WRITE_MEMORY(DivaSafetyWidthPercent, int, HPpercent);
        }
    }

    void prog_hp_reset()
    {
        /*if (HPdenominator == 1)
            return;*/

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
        check_fail(false);
    }

    void check_fail(const bool skip_hp_check)
    {
        if (!skip_hp_check && *(int*)DivaGameHP > 0)
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

    void resetSong()
    {
        APLogger::print("[%6.2f] DeathLink < Auto retry (%i HP / HP Floor: %i, queued: %i)\n",
                        *(float*)DivaGameTimer, *(int*)DivaGameHP, HPfloor, resetQueued);
        PvReset(PvPlayData);
    }

    void run(bool received)
    {
        //if (!APGUI::isInGame()) return;

        auto now = *(float*)DivaGameTimer;

        // Avoid stopping the fade in from white animation at the start of a song and
        // prevents No Fail -> DL to 0 HP -> Return to song select instead of results -> Play
        if (now == 0.0f) {
            reset();
            resetQueued = false;
            return;
        }

        if (resetQueued) {
            resetSong();
            return;
        }

        int currentHP = *(int*)DivaGameHP;

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

        if (!death_link || deathLinked || !received)
            return;

        lastDeathLink = now;

        int hit = (255 - HPfloor) * death_link_percent / 100;
        if (death_link_percent == 50)
            hit += 1;
        else if (death_link_percent == 100)
            hit = 255; // for prog HP and other exceptions. 100% is DEAD.

        int toHP = std::clamp(currentHP - hit, 0, 255);

        deathLinked = toHP <= 0;

        APLogger::print("[%6.2f] DeathLink < death_link_in (%i - %i = %i / DL: %i)\n",
            now, currentHP, hit, toHP, deathLinked);

        setHP(toHP);
    }

    void setHP(int HP)
    {
        HP = std::clamp(HP, 0, 255);
        bool& noFail = *(bool*)(PvPlayData + 0x2D31D);
        const bool& maxCombo = *(int*)(PvPlayData + 0x2D25C) > 0;

        if (HP == 0  && !noFail && auto_retry && APGUI::isInGame() && (deathLinked || maxCombo) /* && death_link */) {
            if (!deathLinked) check_fail(true);
            //if (maxCombo)
            resetQueued = true;
            return;
        }

        WRITE_MEMORY(DivaGameHP, int, HP);
    }

    void ImGuiTab()
    {
        ImGui::PushItemWidth(-1 * (ImGui::GetContentRegionAvail().x * 0.45f));

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

            HelpMarker("Temporarily increase available chunk count.\nResets when the next one is received.");

            if (devMode)
                ImGui::SliderInt("Denominator", &HPdenominator, 1, 20);

            ImGui::Separator();
        }

        ImGui::Checkbox("Automatic retry", &auto_retry);
        HelpMarker("Any death will automatically restart the song.\nSending/receiving Death Links functions the same.");

        if (ImGui::Checkbox("Death Link", &death_link))
            APClient::UpdateTags();
        HelpMarker("When you die on your own or fail to reach Grade Needed (not both), everyone with Death Link enabled dies.");

        if (death_link) {
            if (ImGui::SliderInt("Death Link Amnesty", &death_link_amnesty, 0, 20)) {
                death_link_amnesty = max(0, death_link_amnesty);
                death_link_amnesty_count = death_link_amnesty;
            }
            HelpMarker("Amount of additional own deaths needed before sending one Death Link. 0 would be every death, 1 every other, etc.");

            if (death_link_amnesty > 0) {
                char overlay[64];
                sprintf(overlay, "%d / %d deaths", death_link_amnesty - death_link_amnesty_count, death_link_amnesty);
                ImGui::ProgressBar(static_cast<float>(death_link_amnesty - death_link_amnesty_count) / static_cast<float>(death_link_amnesty), ImVec2(0,0), overlay);
            }

            ImGui::SliderInt("Death Link Percent", &death_link_percent, 0, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
            HelpMarker("Percent of max HP to lose on receive.\n<100 for non-lethal, but makes Life Bonuses harder which may affect score by up to 2%.");

            if (death_link_percent < 100) {
                ImGui::SliderFloat("Death Link Safety", &death_link_safety, 5.0f, 30.0f, "%.1f seconds", ImGuiSliderFlags_AlwaysClamp);
                HelpMarker("Seconds after receiving where dying does not send one out.");
            }

            ImGui::Checkbox("Same slot deaths", &death_link_self);
            HelpMarker("When playing a slot co-op, react to deaths from the same slot.");

            if (devMode)
            {
                ImGui::Separator();

                if (ImGui::Button("100%")) {
                    if (ImGui::GetIO().KeyShift) WRITE_MEMORY(PvPlayData + 0x2D31D, bool, 0);
                    deathLinked = true;
                    setHP(0);
                }
                HelpMarker("Arbitrarily set HP to 0.\n+Shift: unset NoFail");

                ImGui::SameLine();

                if (ImGui::Button("Recv")) {
                    if (ImGui::GetIO().KeyShift) WRITE_MEMORY(PvPlayData + 0x2D31D, bool, 0);
                    deathLinked = false;
                    run(true);
                }
                HelpMarker("Fake receiving a Death Link.\n+Shift: unset NoFail");

                ImGui::SameLine();
                ImGui::Text("Linked: %d", deathLinked);
                HelpMarker("If 1/true, the cause of the death prevented a Death Link from being sent.\nFor example, dying in one hit or inside the safety window.");
            }
        }

        ImGui::PopItemWidth();
    }
}
