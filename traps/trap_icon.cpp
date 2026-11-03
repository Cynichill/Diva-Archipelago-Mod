#include "trap_icon.h"

namespace TrapIcon
{
	void _TrapIcon::config(const toml::table& settings)
	{
		rerollInterval = std::clamp(settings["icon_interval"].value_or(rerollInterval), 0.0f, 60.0f);
		APLogger::print("trap icon_interval: %.02f\n", rerollInterval);

		randomizeGlyphs = settings["icon_glyphs"].value_or(randomizeGlyphs);
		APLogger::print("trap icon_glyphs: %d\n", randomizeGlyphs);

		alternateArrows = settings["icon_arrow_colors"].value_or(alternateArrows);
		APLogger::print("trap icon_arrow_colors: %d\n", alternateArrows);
	}

	void _TrapIcon::save(toml::table& settings)
	{
		settings.insert("icon_interval", rerollInterval);
		settings.insert("icon_glyphs", randomizeGlyphs);
		settings.insert("icon_arrow_colors", alternateArrows);
	}

	void _TrapIcon::reset()
	{
		Trap::reset();
		resetIcon();
	}

	void _TrapIcon::touch()
	{
		timestamp = APTraps::getTrapEndTime(timestamp);
		resetIcon();
		rollIcon();
		running = true;

		APLogger::print("[%6.2f] Trap < Icon (expires: %.2f)\n", now, timestamp);
		//if (timestamp == now) return;
	}

	void _TrapIcon::tick()
	{
		if (!running) return;

		if (now >= timestamp) {
			APLogger::print("[%6.2f] Trap > Icon expired\n", now);
			reset();
			return;
		}

		if (now >= timestampRollNext && rerollInterval > 0.0f) {
			timestampRollNext = now + rerollInterval;
			rollIcon();
		}
	}

	uintptr_t _TrapIcon::getGameControlConfig()
	{

		uintptr_t GCC = reinterpret_cast<uintptr_t(__fastcall*)(void)>(DivaGameControlConfig)();
		return GCC;
	}

	uintptr_t _TrapIcon::getIconAddress()
	{
		return getGameControlConfig() + 0x28;
	}

	int& _TrapIcon::getCurrentIcon()
	{
		return *(int*)getIconAddress();
	}

	void _TrapIcon::resetIcon()
	{
		if (savedIcon == 39) return;

		int restoredIcon = ((savedIcon <= 12 && savedIcon >= 0) ? savedIcon : 4);
		APLogger::print("Traps: Icons restored to %i\n", restoredIcon);
		WRITE_MEMORY(getIconAddress(), int, restoredIcon);
		WRITE_MEMORY(PvControllerGlyphBase, int, restoredIcon);
		savedIcon = 39;
	}

	void _TrapIcon::rollIcon()
	{
		if (!APGUI::isInGame()) return;

		int currentIcon = getCurrentIcon();
		int nextIcon = currentIcon;

		if (savedIcon > 12)
			savedIcon = currentIcon;

		static std::uniform_int_distribution<int> icon_distribution(0, 12); // 0-3 PS, 4 Arrows, 5-8 NSW, 9-12 X

		while (currentIcon == nextIcon) {
			nextIcon = icon_distribution(mt);

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
			int out = icon_distribution(mt);
			WRITE_MEMORY(PvControllerGlyphBase, int, out);
		}
	}

	void _TrapIcon::ImGuiConfig()
	{
		if (ImGui::CollapsingHeader("Icon")) {
			ImGui::SliderFloat("Icon Reroll", &rerollInterval, 0.0f, 60.0f, "%.1f seconds", ImGuiSliderFlags_AlwaysClamp);
			HelpMarker("Seconds between icon rerolls while Icon trap is active.\n0 to only reroll once.");

			ImGui::Checkbox("Icon Trap: Alternate arrow colors", &alternateArrows);
			HelpMarker("When not using random glyphs, allow colored arrows for other controllers.");
			ImGui::Checkbox("Icon Trap: Random controller glyphs", &randomizeGlyphs);

			ImGui::Separator();
		}
	}

	void _TrapIcon::ImGuiStatus()
	{
		if (!running) return;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Icon");
		ImGui::TableNextColumn();
		ImGui::Text("%.02f %i / %i (%i)", timestamp - now, getCurrentIcon(), *(int*)(PvControllerGlyphBase), savedIcon);
	}

	void _TrapIcon::ImGuiExpose()
	{
		if (ImGui::Button("Icon"))
			touch();
	}

	_TrapIcon trap;
}
