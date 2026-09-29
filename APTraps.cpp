#include "APClient.h"
#include "APTraps.h"
#include <deque>
#include "traps/trap.h"
#include "traps/trap_sudden_hidden.h"
#include "traps/trap_sfx.h"
#include "traps/trap_psp.h"
#include "traps/trap_icon.h"
#include "traps/trap_slow.h"
#include "traps/trap_hispeed.h"

namespace APTraps
{
	bool &devMode = APClient::devMode;

	// Config

	float trapDuration = 15.0f;
	bool trapExtendDuration = false; // True: Extend existing trap durations instead of overwriting. NEVER EXTEND STUTTER!
	bool queueTraps = false;
	float queueTrapRate = 0.0f; // +: Wait between traps, 0: apply after previous, -: Wait and overlap

	bool trap_link = false; // Is Trap Link enabled?
	bool trap_link_others = false; // Handle known traps from other games?
	std::vector<std::string> trap_link_tags = { "TrapLink" }; // inb4 Trap Link Groups

	// Traps native to this game. Should map 1:1 with TrapID.
	std::unordered_map<std::string, std::vector<TrapID>> trapMap = {
		{ "Sudden Trap",	{ TrapID::Sudden } },
		{ "Hidden Trap",	{ TrapID::Hidden } },
		{ "Stutter Trap",	{ TrapID::Stutter } },
		{ "Icon Trap",		{ TrapID::Icon } },
		{ "Slow Trap",		{ TrapID::Slow } },
		{ "PSP Trap",		{ TrapID::PSP } },
		{ "SFX Trap",		{ TrapID::SFX } },
		{ "HiSpeed Trap",	{ TrapID::HiSpeed } },
		{ "NoSpeed Trap",	{ TrapID::NoSpeed } },
	};

	// Known traps from other games, when trap_link_others is true
	// Hopefully kept updated: https://docs.google.com/spreadsheets/d/1yoNilAzT5pSU9c2hYK7f2wHAe9GiWDiHFZz8eMe1oeQ/edit?usp=sharing
	std::unordered_map<std::string, std::vector<TrapID>> trapMapExt = {
		{ "144p Trap",				{ TrapID::PSP } },
		{ "Aaa Trap",				{ TrapID::SFX } },
		{ "Banana Trap",			{ TrapID::Stutter } },
		{ "Banana Peel Trap",		{ TrapID::Stutter } },
		{ "Bee Trap",				{ TrapID::HiSpeed } },
		{ "Bonk Trap",				{ TrapID::Stutter } },
		{ "Bubble Trap",			{ TrapID::Stutter } },
		{ "Bullet Time Trap",		{ TrapID::Slow } },
		{ "Camera Trap",			{ TrapID::PSP } },
		{ "Chaos Control Trap",		{ TrapID::Stutter } },
		{ "Chaos Trap",				{ TrapID::Icon } },
		{ "Chart Modifier Trap",	{ TrapID::Icon } },
		{ "Chaser Trap",			{ TrapID::HiSpeed } },
		{ "Confuse Trap",			{ TrapID::Icon } },
		{ "Confound Trap",			{ TrapID::Icon } },
		{ "Confusion Trap",			{ TrapID::Icon } },
		{ "Crystal Trap",			{ TrapID::Stutter } },
		{ "Cutscene Trap",			{ TrapID::Slow } },
		{ "E. Gadd Ramblings",		{ TrapID::SFX } },
		{ "Electrocution Trap",		{ TrapID::Stutter } },
		{ "Exposition Trap",		{ TrapID::SFX } },
		{ "Extreme Chaos Mode",		{ TrapID::Stutter, TrapID::Slow, TrapID::Hidden, TrapID::Sudden, TrapID::Icon } },
		{ "Fake Transition",		{ TrapID::Hidden, TrapID::Sudden } },
		{ "Fast Trap",				{ TrapID::HiSpeed } },
		{ "Fear Trap",				{ TrapID::Sudden } },
		{ "Frame Slime Trap",		{ TrapID::Slow } },
		{ "Freeze Trap",			{ TrapID::Stutter } },
		{ "Frost Trap",				{ TrapID::Stutter } },
		{ "Frozen Trap",			{ TrapID::Stutter } },
		{ "Fuzzy Trap",				{ TrapID::Icon } },
		{ "Gadget Shuffle Trap",	{ TrapID::Icon } },
		{ "Ghost",					{ TrapID::Hidden, TrapID::Sudden } },
		{ "Hiccup Trap",			{ TrapID::Stutter } },
		{ "Honey Trap",				{ TrapID::Slow } },
		{ "Ice Trap",				{ TrapID::Stutter } },
		{ "Input Sequence Trap",	{ TrapID::Icon } },
		{ "Invisiball Trap",		{ TrapID::Hidden, TrapID::Sudden } },
		{ "Invisibility Trap",		{ TrapID::Hidden } },
		{ "Invisible Trap",			{ TrapID::Hidden } },
		{ "Iron Boots Trap",		{ TrapID::Slow } },
		{ "Laughter Trap",			{ TrapID::SFX } },
		{ "Literature Trap",		{ TrapID::SFX } },
		{ "Metronome Trap",			{ TrapID::NoSpeed } },
		{ "Nightmare Trap",			{ TrapID::Hidden, TrapID::Sudden } },
		{ "Ninja Trap",				{ TrapID::Hidden, TrapID::Sudden } },
		{ "Paralysis Trap",			{ TrapID::Stutter } },
		{ "Paralyze Trap",			{ TrapID::Stutter } },
		{ "Paratoad Trap",			{ TrapID::Stutter } },
		{ "Phone Trap",				{ TrapID::SFX } },
		{ "Pie Trap",				{ TrapID::Stutter } },
		{ "Pincercrab Trap",		{ TrapID::Stutter } },
		{ "Pixelate Trap",			{ TrapID::PSP } },
		{ "Pixellation Trap",		{ TrapID::PSP } },
		{ "PowerPoint Trap",		{ TrapID::Slow } },
		{ "Shrink Trap",			{ TrapID::PSP } },
		{ "Shuffle Trap",			{ TrapID::Icon } },
		{ "Sleep Trap",				{ TrapID::Stutter } },
		{ "Slip Trap",				{ TrapID::Stutter } },
		{ "Slowness Trap",			{ TrapID::Slow } },
		{ "Speed Up Trap",			{ TrapID::HiSpeed } },
		{ "Spooky Time",			{ TrapID::Hidden, TrapID::Sudden } },
		{ "Spotlight Trap",			{ TrapID::Hidden, TrapID::Sudden } },
		{ "Sticky Floor Trap",		{ TrapID::Slow } },
		{ "Stun Trap",				{ TrapID::Stutter } },
		{ "Swap Trap",				{ TrapID::Icon } },
		{ "Tar Trap",				{ TrapID::Stutter } },
		{ "Text Trap",				{ TrapID::SFX } },
		{ "Tiny Trap",				{ TrapID::PSP } },
		{ "Vintage Trap",			{ TrapID::PSP, TrapID::Slow } },
		{ "Wailnard",				{ TrapID::Stutter } },
		{ "Yap Trap",				{ TrapID::SFX } },
		{ "Zoom In Trap",			{ TrapID::PSP } },
		{ "Zoom Out Trap",			{ TrapID::PSP } },
		{ "Zoom Trap",				{ TrapID::PSP } },
	};

	//const uint64_t DivaGameModifier = PvPlayData + 0x2D120;
	const uint64_t DivaGameTimer = PvPlayData + 0x2D33C;

	// Internal

	float lastRun = 0.0f; // For delta time against APTraps::DivaGameTimer

	std::deque<TrapID> trapQueue; // TODO

	std::random_device rd;
	std::mt19937 mt;

	std::vector<Trap*>& registeredTraps()
	{
		static std::vector<Trap*> traps;
		return traps;
	}

	void registerTrap(Trap* trap) {
		registeredTraps().push_back(trap);
	}

	void config(const toml::table& settings)
	{
		toml::table section;
		if (settings.contains("traps") && settings["traps"].is_table())
			section = *settings["traps"].as_table();

		trapDuration = std::clamp(section["duration"].value_or(trapDuration), 0.0f, 300.f);
		APLogger::print("trap duration: %.02f\n", trapDuration);

		trapExtendDuration = section["duration_extend"].value_or(trapExtendDuration);
		APLogger::print("trap extend duration: %i\n", trapExtendDuration);

		queueTraps = section["queue"].value_or(queueTraps);
		APLogger::print("trap queue: %d\n", queueTraps);

		queueTrapRate = std::clamp(section["queue_rate"].value_or(queueTrapRate), -30.0f, 30.0f);
		APLogger::print("trap icon_interval: %.02f\n", queueTrapRate);

		trap_link = section["trap_link"].value_or(false);
		APLogger::print("trap_link: %d\n", trap_link);

		trap_link_others = section["trap_link_others"].value_or(trap_link_others);
		APLogger::print("trap_link_others: %d\n", trap_link_others);

		for (Trap* t : registeredTraps()) {
			t->config(section);
		}

		reset();
	}

	void save(toml::table& settings)
	{
		toml::table config;
		config.insert("duration", trapDuration);
		config.insert("duration_extend", trapExtendDuration);
		config.insert("trap_link", trap_link);
		config.insert("trap_link_others", trap_link_others);
		config.insert("queue", queueTraps);
		config.insert("queue_rate", queueTrapRate);

		for (Trap* t : registeredTraps()) {
			t->save(config);
		}

		settings.insert("traps", config);
	}

	int reset()
	{
		APLogger::print("Traps: reset\n");

		lastRun = 0.0f;
		mt.seed(rd());

		for (Trap* t : registeredTraps()) {
			t->reset();
		}

		return 0;
	}

	float& getGameTime()
	{
		return *(float*)DivaGameTimer;
	}

	float getSongLength()
	{
		return *(float*)(PvPlayData + 0x2D338);
	}

	float getTrapEndTime(float& timestampTrap)
	{
		// TODO:
		// Previously returned 0.0f or getSongLength(), when traps were tracked by their start time and went negative instead.
		// getSongLength() is 0.0f by default and not updated until the next song is started.
		// Could recalc on start if 0.0f, or set it ridiculously high here.
		if (trapDuration == 0.0f)
			return 3939.0f;

		auto now = getGameTime();
		return (trapExtendDuration && timestampTrap > 0.0f ? timestampTrap : now) + trapDuration;
	}

	bool canRecv(const int64_t itemID)
	{
		return itemID >= static_cast<int64_t>(TrapID::Hidden) && itemID <= static_cast<int64_t>(TrapID::NoSpeed);
	}

	void trapRecv(const int64_t itemID, const bool notify)
	{
		TrapID trap = static_cast<TrapID>(itemID);

		switch (trap) {
		case TrapID::Hidden:
			if (!notify) return;
			TrapSuhidden::trap.touchSudden();
			linkSend("Hidden Trap");
			break;
		case TrapID::Sudden:
			if (!notify) return;
			TrapSuhidden::trap.touchSudden();
			linkSend("Sudden Trap");
			break;
		case TrapID::Stutter:
			if (!notify) return;
			TrapSlow::trap.touchStutter();
			linkSend("Stutter Trap");
			break;
		case TrapID::Icon:
			if (!notify) return;
			TrapIcon::trap.touch();
			linkSend("Icon Trap");
			break;
		case TrapID::Slow:
			if (!notify) return;
			TrapSlow::trap.touchSlow();
			linkSend("Slow Trap");
			break;
		case TrapID::PSP:
			if (!notify) return;
			TrapPSP::trap.touch();
			linkSend("PSP Trap");
			break;
		case TrapID::HiSpeed:
			if (!notify) return;
			TrapHiSpeed::trap.touchHiSpeed();
			linkSend("HiSpeed Trap");
			break;
		case TrapID::NoSpeed:
			if (!notify) return;
			TrapHiSpeed::trap.touchNoSpeed();
			linkSend("NoSpeed Trap");
			break;
		}
	}

	void linkSend(const std::string& trapName)
	{
		if (!trap_link || !APGUI::isInGame()) return;

		AP_Bounce bounce;
		bounce.tags = &trap_link_tags;
		json data;
		std::chrono::time_point<std::chrono::system_clock> timestamp = std::chrono::system_clock::now();
		data["time"] = (int64_t)std::chrono::duration_cast<std::chrono::seconds>(timestamp.time_since_epoch()).count();
		data["source"] = APClient::getSlotName();
		data["trap_name"] = trapName;

		bounce.data = data.dump();

		AP_SendBounce(bounce);
	}

	void linkRecv(const std::string& trapName)
	{
		if (!trap_link || !APGUI::isInGame() || trapName.empty()) return;

		auto trap = trapMap.find(trapName);

		if (trap_link_others && trap == trapMap.end())
			trap = trapMapExt.find(trapName);

		if (trap == trapMap.end() || trap == trapMapExt.end())
			return;

		const auto& traps = trap->second;
		float now = getGameTime();
		APLogger::print("[%6.2f] Trap < Linked: %s\n", now, trapName.c_str());

		for (auto& trapID : traps) {
			switch (trapID)
			{
			case TrapID::Hidden:
				TrapSuhidden::trap.touchHidden(traps.size() > 1);
				break;
			case TrapID::Sudden:
				TrapSuhidden::trap.touchSudden(traps.size() > 1);
				break;
			case TrapID::Stutter:
				TrapSlow::trap.touchStutter();
				break;
			case TrapID::Icon:
				TrapIcon::trap.touch();
				break;
			case TrapID::Slow:
				TrapSlow::trap.touchSlow();
				break;
			case TrapID::PSP:
				TrapPSP::trap.touch();
				break;
			case TrapID::HiSpeed:
				TrapHiSpeed::trap.touchHiSpeed();
				break;
			case TrapID::NoSpeed:
				TrapHiSpeed::trap.touchNoSpeed();
				break;
			}
		}
	}

	void runQueue()
	{
		float now = getGameTime();
	}

	void runFrame()
	{
		// TODO: These traps disable themselves if the menu is open.
		TrapSlow::trap.tick();
		TrapPSP::trap.tick();
	}

	void run()
	{
		float now = getGameTime();

		if (now == 0.0f && lastRun > 0.0f) {
			reset();
			return;
		}

		if (now - lastRun < 1.0f / 16)
			return;

		lastRun = now;

		for (Trap* t : registeredTraps()) {
			t->tick();
		}
	}

	void ImGuiTab()
	{
		float now = getGameTime();
		float songLength = getSongLength();
		std::string songProgress = std::format("{:.03f} / {:.03f}", now, songLength);
		ImGui::ProgressBar(now / songLength, ImVec2(ImGui::GetContentRegionAvail().x, 0.0f), songProgress.c_str());

		ImGui::SliderFloat("Trap Duration", &trapDuration, 0.0f, 300.0f, "%.1f seconds", ImGuiSliderFlags_AlwaysClamp);
		HelpMarker("Seconds until individual traps expire.\n0 to not expire for current attempt.");

		if (trapDuration > 0.0f) {
			ImGui::Checkbox("Extend trap durations", &trapExtendDuration);
			HelpMarker("Receiving a trap that's already running adds to its duration instead of overwriting it.");
		}

		for (Trap* t : registeredTraps()) {
			//ImGui::PushID(t);
			//if (ImGui::CollapsingHeader("Trap X"))
			t->ImGuiConfig();
			//ImGui::PopID();
		}

		/*
		ImGui::Separator();

		ImGui::Checkbox("Queue Traps", &queueTraps);
		HelpMarker("Instead of applying traps immediately, queue them.\nTrap Link still skips the queue.");

		if (queueTraps) {
			ImGui::SliderFloat("Queue rate", &queueTrapRate, -30.0f, 30.0f, "%.1f seconds", ImGuiSliderFlags_AlwaysClamp);
			HelpMarker("+ Wait between traps\n0 No wait between traps\n- Wait between traps, but overlap");
		}
		*/

		ImGui::Separator();

		if (ImGui::Checkbox("Trap Link", &trap_link))
			APClient::UpdateTags();
		HelpMarker("Share traps with other Trap Link players.\nLinked traps will only apply during play instead of queuing up.\nNote: Traps of the same name from other games will apply regardless of the following option.");

		if (trap_link) {
			ImGui::Checkbox("Traps from other games", &trap_link_others);
			HelpMarker("Handle known traps of a similar effect from other games.\nExample: Ice Trap = Stutter Trap");
			ImGui::SameLine();
			ImGui::TextLinkOpenURL("(?)", "https://docs.google.com/spreadsheets/d/1yoNilAzT5pSU9c2hYK7f2wHAe9GiWDiHFZz8eMe1oeQ/edit?gid=811965759");
		}

		if (devMode) {
			ImGui::Separator();

			if (ImGui::Button("Reset"))
				reset();
			ImGui::SameLine();
			if (ImGui::Button("All"))
				for (Trap* t : registeredTraps()) t->touch();
			HelpMarker("Tempting.");

			for (Trap* t : registeredTraps()) {
				ImGui::PushID(t);

				t->ImGuiExpose();
				ImGui::SameLine();

				ImGui::PopID();
			}
			ImGui::Spacing();

			if (trap_link) {
				static std::string trapName = "";
				ImGui::InputText("##xx", &trapName);
				ImGui::SameLine();
				if (!APGUI::isInGame()) ImGui::BeginDisabled();
				if (ImGui::Button("Trap Link##xx"))
					linkRecv(trapName);
				if (!APGUI::isInGame()) ImGui::EndDisabled();
			}

			if (ImGui::BeginTable("tableTraps", 2))
			{
				for (Trap* t : registeredTraps()) {
					t->ImGuiStatus();
				}

				ImGui::EndTable();
			}
		}
	}
}
