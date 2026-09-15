#include "APClient.h"
#include "APTraps.h"
#include "APGUI.h"
#include <deque>

namespace APTraps
{
	bool &devMode = APClient::devMode;

	// Config

	float trapDuration = 15.0f;
	float iconInterval = 60.0f;
	bool trapOverlap = false;
	bool trapExtendDuration = false; // True: Extend existing trap durations instead of overwriting. NEVER EXTEND STUTTER!
	bool randomizeGlyphs = false;
	bool alternateArrows = true;
	int slowTarget = 30;
	bool queueTraps = false;
	float queueTrapRate = 0.0f; // +: Wait between traps, 0: apply after previous, -: Wait and overlap
	int pspHeight = 270; // While isPSP, use this resolution height (scaled to 16:9)

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
		{ "PSP Trap",		{ TrapID::PSP } }
	};

	// Known traps from other games, when trap_link_others is true
	// Hopefully kept updated: https://docs.google.com/spreadsheets/d/1yoNilAzT5pSU9c2hYK7f2wHAe9GiWDiHFZz8eMe1oeQ/edit?usp=sharing
	std::unordered_map<std::string, std::vector<TrapID>> trapMapExt = {
		{ "144p Trap",				{ TrapID::PSP } },
		{ "Banana Trap",			{ TrapID::Stutter } },
		{ "Banana Peel Trap",		{ TrapID::Stutter } },
		{ "Bonk Trap",				{ TrapID::Stutter } },
		{ "Bullet Time Trap",		{ TrapID::Slow } },
		{ "Camera Trap",			{ TrapID::PSP } },
		{ "Chaos Control Trap",		{ TrapID::Stutter } },
		{ "Chaos Trap",				{ TrapID::Icon } },
		{ "Chart Modifier Trap",	{ TrapID::Icon } },
		{ "Confuse Trap",			{ TrapID::Icon } },
		{ "Confound Trap",			{ TrapID::Icon } },
		{ "Confusion Trap",			{ TrapID::Icon } },
		{ "Cutscene Trap",			{ TrapID::Slow } },
		{ "Extreme Chaos Mode",		{ TrapID::Stutter, TrapID::Slow, TrapID::Hidden, TrapID::Sudden, TrapID::Icon } },
		{ "Fake Transition",		{ TrapID::Hidden, TrapID::Sudden } },
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
		{ "Invisibility Trap",		{ TrapID::Hidden } },
		{ "Invisible Trap",			{ TrapID::Hidden } },
		{ "Iron Boots Trap",		{ TrapID::Slow } },
		{ "Nightmare Trap",			{ TrapID::Hidden, TrapID::Sudden } },
		{ "Paralysis Trap",			{ TrapID::Stutter } },
		{ "Paralyze Trap",			{ TrapID::Stutter } },
		{ "Paratoad Trap",			{ TrapID::Stutter } },
		{ "Pie Trap",				{ TrapID::Stutter } },
		{ "Pixelate Trap",			{ TrapID::PSP } },
		{ "Pixellation Trap",		{ TrapID::PSP } },
		{ "PowerPoint Trap",		{ TrapID::Slow } },
		{ "Shrink Trap",			{ TrapID::PSP } },
		{ "Shuffle Trap",			{ TrapID::Icon } },
		{ "Sleep Trap",				{ TrapID::Stutter } },
		{ "Slip Trap",				{ TrapID::Stutter } },
		{ "Slowness Trap",			{ TrapID::Slow } },
		{ "Spooky Time",			{ TrapID::Hidden, TrapID::Sudden } },
		{ "Stun Trap",				{ TrapID::Stutter } },
		{ "Swap Trap",				{ TrapID::Icon } },
		{ "Tiny Trap",				{ TrapID::PSP } },
		{ "Wailnard",				{ TrapID::Stutter } },
		{ "Zoom In Trap",			{ TrapID::PSP } },
		{ "Zoom Out Trap",			{ TrapID::PSP } },
		{ "Zoom Trap",				{ TrapID::PSP } },
	};

	const uint64_t DivaGameControlConfig = 0x1401D6520;
	const uint64_t PvControllerGlyphBase = 0x141133D30; // Copy of GCC Icon on load (0-12), original caller returns base glyph (0-2).
	//const uint64_t DivaGameModifier = PvPlayData + 0x2D120;
	const uint64_t DivaGameTimer = PvPlayData + 0x2D33C;

	// Internal

	float lastRun = 0.0f; // For delta time against APTraps::DivaGameTimer

	int savedIcon = 39; // If randomizeGlyphs: also used to restore glyphs
	bool isSudden = false; // Had trouble with this as a bool(timestamp > 0)
	bool isHidden = false; // Had trouble with this as a bool(timestamp > 0)
	bool isIcon = false;
	bool isStutter = false;
	bool isSlow = false;
	bool isPSP = false;
	int stutterTarget = 10; // FPS that Stutter drops to briefly. Too low can get the trap "stuck" on for longer than intended.
	int prevFramerate = 0; // Target framerate before Stutter or Slow overwrite it. The game applies 0 as "uncapped", then considers vsync.
	Resolution prevRes;

	std::deque<TrapID> trapQueue; // TODO

	float timestampSudden = 0.0f; // Time the trap expires
	float timestampHidden = 0.0f; // Time the trap expires
	float timestampIcon = 0.0f; // Time the trap expires
	float timestampIconNext = 0.0f; // Time the icon should be rolled again
	float timestampStutter = 0.0f; // Time the trap expires
	float timestampSlow = 0.0f; // Time the trap expires
	float timestampPSP = 0.0f;

	std::mt19937 mt;
	std::uniform_int_distribution<int> dist(0, 12); // 0-3 PS, 4 Arrows, 5-8 NSW, 9-12 X

	auto adjustViewport = reinterpret_cast<void(__fastcall*)(void* p1, int width, int height, void* p4)>(0x1402C27E0);

	void config(const toml::table& settings)
	{
		toml::table section;
		if (settings.contains("traps") && settings["traps"].is_table())
			section = *settings["traps"].as_table();

		trapDuration = std::clamp(section["duration"].value_or(trapDuration), 0.0f, 300.f);
		APLogger::print("trap duration: %.02f\n", trapDuration);

		trapExtendDuration = section["duration_extend"].value_or(trapExtendDuration);
		APLogger::print("trap extend duration: %i\n", trapExtendDuration);

		iconInterval = std::clamp(section["icon_interval"].value_or(iconInterval), 0.0f, 60.0f);
		APLogger::print("trap icon_interval: %.02f\n", iconInterval);

		slowTarget = std::clamp(section["slow_target"].value_or(slowTarget), 15, 60);
		APLogger::print("slow_target: %i\n", slowTarget);

		trapOverlap = section["overlap"].value_or(trapOverlap);
		APLogger::print("trap overlap: %d\n", trapOverlap);

		queueTraps = section["queue"].value_or(queueTraps);
		APLogger::print("trap queue: %d\n", queueTraps);

		queueTrapRate = std::clamp(section["queue_rate"].value_or(queueTrapRate), -30.0f, 30.0f);
		APLogger::print("trap icon_interval: %.02f\n", queueTrapRate);

		trap_link = section["trap_link"].value_or(false);
		APLogger::print("trap_link: %d\n", trap_link);

		trap_link_others = section["trap_link_others"].value_or(trap_link_others);
		APLogger::print("trap_link_others: %d\n", trap_link_others);

		randomizeGlyphs = section["icon_glyphs"].value_or(randomizeGlyphs);
		APLogger::print("trap icon_glyphs: %d\n", randomizeGlyphs);

		alternateArrows = section["icon_arrow_colors"].value_or(alternateArrows);
		APLogger::print("trap icon_arrow_colors: %d\n", alternateArrows);

		pspHeight = std::clamp(section["psp_height"].value_or(pspHeight), 45, 544);
		APLogger::print("trap psp_height: %i\n", pspHeight);
	}

	void save(toml::table& settings)
	{
		toml::table config;
		config.insert("duration", trapDuration);
		config.insert("duration_extend", trapExtendDuration);
		config.insert("icon_interval", iconInterval);
		config.insert("icon_glyphs", randomizeGlyphs);
		config.insert("icon_arrow_colors", alternateArrows);
		config.insert("slow_target", slowTarget);
		config.insert("overlap", trapOverlap);
		config.insert("trap_link", trap_link);
		config.insert("trap_link_others", trap_link_others);
		config.insert("psp_height", pspHeight);
		config.insert("queue", queueTraps);
		config.insert("queue_rate", queueTrapRate);

		settings.insert("traps", config);
	}

	void resetFramerate()
	{
		if (prevFramerate > 0) {
			int* framerate = reinterpret_cast<int*>(0x1414ABBB8);
			*framerate = max(prevFramerate, 30);
			prevFramerate = 0;
		}
	}

	int reset()
	{
		APLogger::print("Traps: reset\n");

		lastRun = 0.0f;

		resetIcon();
		timestampSudden = 0.0f;
		timestampHidden = 0.0f;
		timestampIcon = 0.0f;
		timestampIconNext = 0.0f;
		timestampStutter = 0.0;
		timestampSlow = 0.0f;
		timestampPSP = 0.0f;
		isHidden = false;
		isSudden = false;
		isStutter = false;
		isSlow = false;
		isPSP = false;

		runPSP();
		resetFramerate();

		return 0;
	}

	void resetIcon()
	{
		if (savedIcon == 39) return;

		int restoredIcon = ((savedIcon <= 12 && savedIcon >= 0) ? savedIcon : 4);
		APLogger::print("Traps: Icons restored to %i\n", restoredIcon);
		WRITE_MEMORY(getIconAddress(), int, restoredIcon);
		WRITE_MEMORY(PvControllerGlyphBase, int, restoredIcon);
		savedIcon = 39;
	}

	float getGameTime()
	{
		return *(float*)DivaGameTimer;
	}

	float getSongLength()
	{
		return *(float*)(PvPlayData + 0x2D338);
	}

	float getTrapEndTime(float& timestampTrap)
	{
		if (trapDuration == 0.0f)
			return getSongLength();

		auto now = getGameTime();
		return (trapExtendDuration && timestampTrap > 0.0f ? timestampTrap : now) + trapDuration;
	}

	void touchSudden()
	{
		touchSudden(false);
	}

	void touchSudden(bool force)
	{
		float now = getGameTime();
		timestampSudden = getTrapEndTime(timestampSudden);

		APLogger::print("[%6.2f] Trap < Sudden (expires: %.2f)\n", now, timestampSudden);
		isSudden = true;

		bool overlap = force || trapOverlap;

		if (!overlap && isHidden) {
			APLogger::print("[%6.2f] Trap < Hidden -> Sudden (expires: %.2f)\n", now, timestampSudden);
			timestampHidden = 0.0f;
			isHidden = false;
		}
	}

	void touchHidden()
	{
		touchHidden(false);
	}

	void touchHidden(bool force)
	{
		float now = getGameTime();
		timestampHidden = getTrapEndTime(timestampHidden);

		APLogger::print("[%6.2f] Trap < Hidden (expires: %.2f)\n", now, timestampHidden);
		isHidden = true;

		bool overlap = force || trapOverlap;

		if (!overlap && isSudden) {
			APLogger::print("[%6.2f] Trap < Sudden -> Hidden (expires: %.2f)\n", now, timestampHidden);
			timestampSudden = 0.0f;
			isSudden = false;
		}
	}

	void touchStutter()
	{
		float now = getGameTime();
		APLogger::print("[%6.2f] Trap < Stutter\n", now);

		isStutter = true;
		// NEVER extend duration. Easy DoS.
		timestampStutter = now + 0.5f;
	}

	void touchIcon()
	{
		float now = getGameTime();
		timestampIcon = getTrapEndTime(timestampIcon);

		resetIcon();

		APLogger::print("[%6.2f] Trap < Icon (expires: %.2f)\n", now, timestampIcon);
		rollIcon();
		isIcon = true;

		if (timestampIcon == now)
			return;
	}

	void touchSlow()
	{
		float now = getGameTime();
		timestampSlow = getTrapEndTime(timestampSlow);

		APLogger::print("[%6.2f] Trap < Slow (expires: %.2f)\n", now, timestampSlow);
		isSlow = true;
	}

	void touchPSP()
	{
		float now = getGameTime();
		timestampPSP = getTrapEndTime(timestampPSP);

		if (!isPSP)
			prevRes.update();

		APLogger::print("[%6.2f] Trap < PSP (expires: %.2f, %i x %i)\n", now, timestampPSP, prevRes.width, prevRes.height);
		isPSP = true;
	}

	void runPSP()
	{
		int* currentHeight = (int*)(*(prevRes.path) + 0x44);
		if (APGUI::isInGame() && isPSP) {
			if (*currentHeight == pspHeight) return;

			ImGui::SetWindowFocus(nullptr); // The client is going to be unusable anyway.
			adjustViewport(nullptr, pspHeight % 272 == 0 ? 480 * pspHeight / 272 : prevRes.width * pspHeight / prevRes.height, pspHeight, nullptr);
		}
		else if (prevRes.height > 0 && prevRes.height != *currentHeight) {
			adjustViewport(nullptr, prevRes.width, prevRes.height, nullptr);
		}
	}

	bool canRecv(const int64_t itemID)
	{
		return itemID >= static_cast<int64_t>(TrapID::Hidden) && itemID <= static_cast<int64_t>(TrapID::PSP);
	}

	void trapRecv(const int64_t itemID, const bool notify)
	{
		TrapID trap = static_cast<TrapID>(itemID);

		switch (trap) {
		case TrapID::Hidden:
			if (!notify) return;
			touchHidden();
			linkSend("Hidden Trap");
			break;
		case TrapID::Sudden:
			if (!notify) return;
			touchSudden();
			linkSend("Sudden Trap");
			break;
		case TrapID::Stutter:
			if (!notify) return;
			touchStutter();
			linkSend("Stutter Trap");
			break;
		case TrapID::Icon:
			if (!notify) return;
			touchIcon();
			linkSend("Icon Trap");
			break;
		case TrapID::Slow:
			if (!notify) return;
			touchSlow();
			linkSend("Slow Trap");
			break;
		case TrapID::PSP:
			if (!notify) return;
			touchPSP();
			linkSend("PSP Trap");
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
				touchHidden(traps.size() > 1);
				break;
			case TrapID::Sudden:
				touchSudden(traps.size() > 1);
				break;
			case TrapID::Stutter:
				touchStutter();
				break;
			case TrapID::Icon:
				touchIcon();
				break;
			case TrapID::Slow:
				touchSlow();
				break;
			case TrapID::PSP:
				touchPSP();
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
		runSlow();
		runPSP();
	}

	void run()
	{
		float now = getGameTime();

		if (now == 0.0f && lastRun > 0.0f) {
			reset();
			return;
		}

		if (now - lastRun < 0.1f)
			return;

		lastRun = now;

		if (isSudden && now >= timestampSudden) {
			APLogger::print("[%6.2f] Trap > Sudden expired\n", now);
			timestampSudden = 0.0f;
			isSudden = false;
		}

		if (isHidden && now >= timestampHidden) {
			APLogger::print("[%6.2f] Trap > Hidden expired\n", now);
			timestampHidden = 0.0f;
			isHidden = false;
		}

		if (isIcon) {
			if (now >= timestampIcon) {
				APLogger::print("[%6.2f] Trap > Icon expired\n", now);
				timestampIcon = 0.0f;
				isIcon = false;
				resetIcon();
			}
			else {
				if (now >= timestampIconNext && iconInterval > 0.0f) {
					timestampIconNext = now + iconInterval;
					rollIcon();
				}
			}
		}

		if (isPSP && now >= timestampPSP) {
			APLogger::print("[%6.2f] Trap > PSP expired\n", now);
			timestampPSP = 0.0f;
			isPSP = false;
		}
	}

	void runSlow()
	{
		if (APGUI::isInGame() && (isStutter || isSlow)) {
			float now = getGameTime();
			int* framerate = reinterpret_cast<int*>(0x1414ABBB8);
			int target = 60;

			if (isStutter) {
				if (now >= timestampStutter) {
					APLogger::print("[%6.2f] Trap > Stutter expired\n", now);
					timestampStutter = 0.0f;
					isStutter = false;

					resetFramerate();
					return;
				}

				target = stutterTarget;
			} else if (isSlow) {
				if (now >= timestampSlow) {
					APLogger::print("[%6.2f] Trap > Slow expired\n", now);
					timestampSlow = 0.0f;
					isSlow = false;

					resetFramerate();
					return;
				}

				target = slowTarget;
			}

			if (prevFramerate == 0)
				prevFramerate = *framerate;

			if (*framerate != target)
				*framerate = target;
		}
		else {
			resetFramerate();
		}
	}

	uint64_t getGameControlConfig()
	{
		uint64_t GCC = reinterpret_cast<uint64_t(__fastcall*)(void)>(DivaGameControlConfig)();
		return GCC;
	}

	uint64_t getIconAddress()
	{
		return getGameControlConfig() + 0x28;
	}

	int getCurrentIcon()
	{
		return *(int*)getIconAddress();
	}

	void rollIcon()
	{
		if (!APGUI::isInGame()) return;

		int currentIcon = getCurrentIcon();
		int nextIcon = currentIcon;

		if (savedIcon > 12)
			savedIcon = currentIcon;

		while (currentIcon == nextIcon) {
			nextIcon = dist(mt);

			if (!alternateArrows) {
				if (currentIcon <= 3)
					nextIcon %= 4;
				else if (currentIcon >= 5 && currentIcon <= 8)
					nextIcon = 5 + (nextIcon % 4);
				else if (currentIcon >= 9)
					nextIcon = 9 + (nextIcon % 4);
				else // 4
					nextIcon = savedIcon;
			}
		}

		WRITE_MEMORY(getIconAddress(), int, nextIcon);

		if (randomizeGlyphs) {
			int out = dist(mt);
			WRITE_MEMORY(PvControllerGlyphBase, int, out);
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

		ImGui::SliderFloat("Icon Reroll", &iconInterval, 0.0f, 60.0f, "%.1f seconds", ImGuiSliderFlags_AlwaysClamp);
		HelpMarker("Seconds between icon rerolls while Icon trap is active.\n0 to only reroll once.");

		if (ImGui::SliderInt("Slow FPS", &slowTarget, 20, 40))
			slowTarget = std::clamp(slowTarget, 15, 60);
		HelpMarker("Chain Slides may have issues below 30 FPS, based on speed.");

		std::string res = std::format("{}x{}", pspHeight % 272 == 0 ? 480 * pspHeight / 272 : pspHeight * 16 / 9, pspHeight);
		if (ImGui::SliderInt("PSP resolution", &pspHeight, 90, 272, res.c_str()))
			pspHeight = std::clamp(pspHeight, 45, 544);
		HelpMarker("Resolution for the PSP Trap.\nBlurry? Try a display mode other than \"Fullscreen\".");

		ImGui::Checkbox("Allow Sudden and Hidden to overlap", &trapOverlap);
		ImGui::Checkbox("Icon Trap: Alternate arrow colors", &alternateArrows);
		HelpMarker("When not using random glyphs, allow colored arrows for other controllers.");
		ImGui::Checkbox("Icon Trap: Random controller glyphs", &randomizeGlyphs);

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
			if (ImGui::Button("Sudden"))
				touchSudden();
			ImGui::SameLine();
			if (ImGui::Button("Hidden"))
				touchHidden();
			ImGui::SameLine();
			if (ImGui::Button("Icon"))
				touchIcon();

			if (ImGui::Button("Stutter"))
				touchStutter();
			ImGui::SameLine();
			if (ImGui::Button("Slow"))
				touchSlow();
			ImGui::SameLine();
			if (ImGui::Button("PSP"))
				touchPSP();

			if (trap_link) {
				static char tl[20];
				ImGui::InputText("##xx", tl, sizeof(tl));
				ImGui::SameLine();
				if (!APGUI::isInGame()) ImGui::BeginDisabled();
				if (ImGui::Button("Trap Link##xx"))
					linkRecv(std::string(tl));
				if (!APGUI::isInGame()) ImGui::EndDisabled();
			}

			ImGui::SliderInt("Stutter FPS", &stutterTarget, 1, 10, NULL, ImGuiSliderFlags_AlwaysClamp);

			if (ImGui::BeginTable("tableTraps", 2))
			{
				if (isSudden)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("Sudden");
					ImGui::TableNextColumn();
					ImGui::Text("%.02f", timestampSudden - now);
				}

				if (isHidden)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("Hidden");
					ImGui::TableNextColumn();
					ImGui::Text("%.02f", timestampHidden - now);
				}

				if (isStutter)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("Stutter");
					ImGui::TableNextColumn();
					ImGui::Text("%.02f (%i > %i FPS)", timestampStutter - now, prevFramerate, stutterTarget);
				}

				if (isSlow)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("Slow");
					ImGui::TableNextColumn();
					ImGui::Text("%.02f (%i > %i FPS)", timestampSlow - now, prevFramerate, slowTarget);
				}

				if (isIcon)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("Icon");
					ImGui::TableNextColumn();
					ImGui::Text("%.02f %i / %i (%i)", timestampIcon - now, getCurrentIcon(), *(int*)(PvControllerGlyphBase), savedIcon);
				}

				if (isPSP)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("PSP");
					ImGui::TableNextColumn();
					ImGui::Text("%.02f %i x %i", timestampPSP - now, prevRes.width, prevRes.height);
				}

				ImGui::EndTable();
			}
		}
	}
}
