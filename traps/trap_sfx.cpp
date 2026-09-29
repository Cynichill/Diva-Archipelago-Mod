#include "trap_sfx.h"
#include "..\Diva.h"
#include <cstring>

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
			StringInit(&pvplaydata_sfx.se_name, prevButton, strlen(prevButton));
			StringInit(&pvplaydata_sfx.pvbranch_success_se_name, prevChance, strlen(prevChance));
			StringInit(&pvplaydata_sfx.slide_name, prevSlide, strlen(prevSlide));
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
		if (strlen(prevButton) == 0 && pvplaydata_sfx.se_name.length > 0) {
			std::strcpy(prevButton, pvplaydata_sfx.se_name.data());
			std::strcpy(prevChance, pvplaydata_sfx.se_name.data());
			std::strcpy(prevSlide, pvplaydata_sfx.slide_name.data());
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

		// Roll these
		static std::vector<DivaString*> sfx = {
			&pvplaydata_sfx.se_name,
			&pvplaydata_sfx.pvbranch_success_se_name,
			&pvplaydata_sfx.slide_name,
			&pvplaydata_sfx.chainslide_first_name,
			&pvplaydata_sfx.chainslide_sub_name,
			&pvplaydata_sfx.chainslide_success_name,
			&pvplaydata_sfx.chainslide_failure_name,
		};

		// To these
		for (auto &s : sfx) {
			std::uniform_int_distribution<int> dist(0, 2);
			auto roll = dist(mt);
			const char* out = sfx_button_list->data[0].file.data();

			if (roll == 0) { // Pick from buttons
				out = sfx_button_list->data[sfx_button_dist(mt)].file.data();
			}
			else if (roll == 1) { // Slides
				out = sfx_slide_list->data[sfx_slide_dist(mt)].file.data();
			}
			else if (roll == 2) { // Chainslides
				std::uniform_int_distribution<int> chaindist(1, 2); // Skip certain ones for now/ever.
				auto chainroll = chaindist(mt);
				auto chainslide = sfx_chainslide_list->data[sfx_chainslide_dist(mt)];

				if (chainroll == 0) {
					out = chainslide.chainslide_first_name.data();
				}
				else if (chainroll == 1) {
					out = chainslide.chainslide_sub_name.data();
				}
				else if (chainroll == 2) {
					out = chainslide.chainslide_success_name.data();
				}
				else if (chainroll == 3) {
					out = chainslide.chainslide_failure_name.data();
				}
			}
			//else if (roll == 3) {
			//	// TODO: Player provided?
			//	static std::vector<std::string> x = {
			//		//"pvchange01", "pvchange02", "pvchange03", "pvchange04",
			//	};
			//	std::uniform_int_distribution<> xdist(0, x.size() - 1);
			// x[xdist(mt)].c_str();
			//}

			std::string name(out);
			StringInit(s, name.c_str(), strlen(name.c_str()));
		}
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
