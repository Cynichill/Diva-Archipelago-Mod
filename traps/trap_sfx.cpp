#include "trap_sfx.h"
#include "..\Diva.h"

namespace TrapSFX
{
	void _TrapSFX::config(const toml::table& settings)
	{
		rerollInterval = std::clamp(settings["sfx_interval"].value_or(rerollInterval), 0.0f, 60.0f);
		APLogger::print("trap sfx_interval: %.02f\n", rerollInterval);
	}

	void _TrapSFX::save(toml::table& settings)
	{
		settings.insert("sfx_interval", rerollInterval);
	}

	void _TrapSFX::resetSFX()
	{
		PvPlayData_SFX& pvplaydata_sfx = *(PvPlayData_SFX*)(PvPlayData + 0x2CF18);

		if (strlen(prevButton) > 0) {
			StringInit(&pvplaydata_sfx.button, prevButton, strlen(prevButton));
			StringInit(&pvplaydata_sfx.slide, prevSlide, strlen(prevSlide));
			StringInit(&pvplaydata_sfx.chainslide_first_name, prevChainFirst, strlen(prevChainFirst));
			StringInit(&pvplaydata_sfx.chainslide_sub_name, prevChainSub, strlen(prevChainSub));
			StringInit(&pvplaydata_sfx.chainslide_success_name, prevChainSuccess, strlen(prevChainSuccess));
			StringInit(&pvplaydata_sfx.chainslide_failure_name, prevChainFailure, strlen(prevChainFailure));

			prevButton[0] = '\0';
		}
	}

	void _TrapSFX::reset()
	{
		Trap::reset();

		timestampRollNext = 0.0f;

		resetSFX();
	}

	void _TrapSFX::touch()
	{
		resetSFX();
		timestamp = APTraps::getTrapEndTime(timestamp);
		running = true;

		APLogger::print("[%6.2f] Trap < SFX (expires: %.2f)\n", now, timestamp);
	}

	void _TrapSFX::tick()
	{
		if (!running) return;

		if (now >= timestamp) {
			APLogger::print("[%6.2f] Trap > SFX expired\n", now);
			reset();
			return;
		}

		PvPlayData_SFX& pvplaydata_sfx = *(PvPlayData_SFX*)(PvPlayData + 0x2CF18);

		// Store during an in-game tick to not grab empty values on touch().
		// TODO: Fetch from original source properly.
		if (strlen(prevButton) == 0 && pvplaydata_sfx.button.length > 0) {
			std::strcpy(prevButton, pvplaydata_sfx.button.data());
			std::strcpy(prevSlide, pvplaydata_sfx.slide.data());
			std::strcpy(prevChainFirst, pvplaydata_sfx.chainslide_first_name.data());
			std::strcpy(prevChainSub, pvplaydata_sfx.chainslide_sub_name.data());
			std::strcpy(prevChainSuccess, pvplaydata_sfx.chainslide_success_name.data());
			std::strcpy(prevChainFailure, pvplaydata_sfx.chainslide_failure_name.data());
		}

		if (now <= timestampRollNext) return;

		timestampRollNext = now + rerollInterval;

		static SFXList* sfx_button_list = reinterpret_cast<SFXList*>(0x1416E24C0);
		static std::uniform_int_distribution<int> sfx_button_dist(0, sfx_button_list->count - 1);

		static SFXList* sfx_slide_list = reinterpret_cast<SFXList*>(0x1416E4A40);
		static std::uniform_int_distribution<int> sfx_slide_dist(0, sfx_slide_list->count - 1);

		static SFXChainslideList* sfx_chainslide_list = reinterpret_cast<SFXChainslideList*>(0x1416E2590);
		static std::uniform_int_distribution<int> sfx_chainslide_dist(0, sfx_chainslide_list->count - 1);

		auto& next_button = sfx_button_list->data[sfx_button_dist(mt)];
		auto& next_slide = sfx_button_list->data[sfx_slide_dist(mt)];

		// TODO: Even more cross-note shuffling. Who says a chain slide can't have button sounds?

		std::uniform_int_distribution<int> roll_base(0, 2);
		if (roll_base(mt) == 0) { // Original
			StringInit(&pvplaydata_sfx.button, next_button.file.data(), next_button.file.length);
			StringInit(&pvplaydata_sfx.slide, next_slide.file.data(), next_slide.file.length);
		}
		else { // Swap
			StringInit(&pvplaydata_sfx.button, next_slide.file.data(), next_slide.file.length);
			StringInit(&pvplaydata_sfx.slide, next_button.file.data(), next_button.file.length);
		}

		auto& chainslide = sfx_chainslide_list->data[sfx_chainslide_dist(mt)];
		StringInit(&pvplaydata_sfx.chainslide_first_name, chainslide.chainslide_first_name.data(), chainslide.chainslide_first_name.length);
		StringInit(&pvplaydata_sfx.chainslide_sub_name, chainslide.chainslide_sub_name.data(), chainslide.chainslide_sub_name.length);
		StringInit(&pvplaydata_sfx.chainslide_success_name, chainslide.chainslide_success_name.data(), chainslide.chainslide_success_name.length);
		StringInit(&pvplaydata_sfx.chainslide_failure_name, chainslide.chainslide_failure_name.data(), chainslide.chainslide_failure_name.length);
	}

	void _TrapSFX::ImGuiConfig()
	{
		ImGui::SliderFloat("SFX Reroll", &rerollInterval, 0.0f, 60.0f, "%.1f seconds", ImGuiSliderFlags_AlwaysClamp);
		HelpMarker("Seconds between SFX rerolls while SFX trap is active.\n0 to only reroll once.");
	}

	void _TrapSFX::ImGuiStatus()
	{
		if (!running) return;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("SFX");
		ImGui::TableNextColumn();
		ImGui::Text("%.02f %s", timestamp - now, prevButton);
	}

	void _TrapSFX::ImGuiExpose()
	{
		if (ImGui::Button("SFX"))
			touch();
	}

	_TrapSFX trap;
}
