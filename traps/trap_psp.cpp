#include "..\APTraps.h"
#include "trap_psp.h"

auto adjustViewport = reinterpret_cast<void(__fastcall*)(void* p1, int width, int height, void* p4)>(0x1402C27E0);

void TrapPSP::config(const toml::table& settings)
{
	height = std::clamp(settings["psp_height"].value_or(height), 45, 544);
	APLogger::print("trap psp_height: %i\n", height);
}

void TrapPSP::save(toml::table& settings)
{
	settings.insert("psp_height", height);
}

void TrapPSP::reset()
{
	Trap::reset();

	int& currentHeight = *(int*)(*(prevRes.path) + 0x44);
	if (prevRes.height > 0 && prevRes.height != currentHeight) {
		adjustViewport(nullptr, prevRes.width, prevRes.height, nullptr);
		prevRes.clear();
	}
}

void TrapPSP::touch()
{
	reset();

	if (!running)
		prevRes.update();

	running = true;
	timestamp = APTraps::getTrapEndTime(timestamp);

	APLogger::print("[%6.2f] Trap < PSP (expires: %.2f, %i x %i)\n", now, timestamp, prevRes.width, prevRes.height);
}

void TrapPSP::tick()
{
	if (!running) return;

	if (now >= timestamp) {
		APLogger::print("[%6.2f] Trap > PSP expired\n", now);
		reset();
		return;
	}

	int& currentHeight = *(int*)(*(prevRes.path) + 0x44);
	if (APGUI::isInGame()) {
		if (currentHeight == height) return;

		ImGui::SetWindowFocus(nullptr); // The client is going to be unusable anyway.
		adjustViewport(nullptr, height % 272 == 0 ? 480 * height / 272 : prevRes.width * height / prevRes.height, height, nullptr);
	}
	else if (prevRes.height > 0 && prevRes.height != currentHeight) {
		adjustViewport(nullptr, prevRes.width, prevRes.height, nullptr);
	}
}

void TrapPSP::ImGuiConfig()
{
	std::string res = std::format("{}x{}", height % 272 == 0 ? 480 * height / 272 : height * 16 / 9, height);
	if (ImGui::SliderInt("PSP resolution", &height, 90, 272, res.c_str()))
		height = std::clamp(height, 45, 544);
	HelpMarker("Resolution for the PSP Trap.\nBlurry? Try a display mode other than \"Fullscreen\".");
}

void TrapPSP::ImGuiStatus()
{
	if (!running) return;

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::Text("PSP");
	ImGui::TableNextColumn();
	ImGui::Text("%.02f %i x %i", timestamp - now, prevRes.width, prevRes.height);
}
