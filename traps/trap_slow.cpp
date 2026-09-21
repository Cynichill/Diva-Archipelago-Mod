#include "trap_slow.h"

void TrapSlow::config(const toml::table& settings)
{
	slowTarget = std::clamp(settings["slow_target"].value_or(slowTarget), 15, 60);
	APLogger::print("slow_target: %i\n", slowTarget);
}

void TrapSlow::save(toml::table& settings)
{
	settings.insert("slow_target", slowTarget);
}

void TrapSlow::reset()
{
	Trap::reset();

	resetFramerate();
}

void TrapSlow::touch(bool stutter)
{
	if (stutter) {
		touchStutter();
		return;
	}

	running = true;
	timestamp = APTraps::getTrapEndTime(timestamp);
	APLogger::print("[%6.2f] Trap < Slow (expires: %.2f)\n", now, timestamp);
}

void TrapSlow::touch()
{
	touch(false);
}

void TrapSlow::touchStutter()
{
	isStutter = true;

	// NEVER extend duration. Easy DoS.
	timestampStutter = now + 0.5f;

	APLogger::print("[%6.2f] Trap < Stutter (expires: %.2f)\n", now, timestampStutter);
}

void TrapSlow::tick()
{
	if (APGUI::isInGame() && (isStutter || running)) {
		int& framerate = *reinterpret_cast<int*>(0x1414ABBB8);
		int target = slowTarget;

		if (isStutter) {
			if (now >= timestampStutter) {
				APLogger::print("[%6.2f] Trap > Stutter expired\n", now);
				timestampStutter = 0.0f;
				isStutter = false;

				resetFramerate();
				return;
			}

			target = stutterTarget;
		}
		else if (running && now >= timestamp) {
			APLogger::print("[%6.2f] Trap > Slow expired\n", now);
			reset();
			return;
		}

		if (prevFramerate == 0)
			prevFramerate = framerate;

		if (framerate != target)
			framerate = target;
	}
	else {
		resetFramerate();
	}
}

void TrapSlow::resetFramerate()
{
	if (prevFramerate > 0) {
		int& framerate = *reinterpret_cast<int*>(0x1414ABBB8);
		framerate = max(prevFramerate, 30);
		prevFramerate = 0;
	}
}

void TrapSlow::ImGuiConfig()
{
	if (ImGui::SliderInt("Slow FPS", &slowTarget, 20, 40))
		slowTarget = std::clamp(slowTarget, 15, 60);
	HelpMarker("Chain Slides may have issues below 30 FPS, based on speed.");

	if (devMode) {
		ImGui::SliderInt("Stutter FPS", &stutterTarget, 1, 10, NULL, ImGuiSliderFlags_AlwaysClamp);
	}
}

void TrapSlow::ImGuiStatus()
{
	if (isStutter) {
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Stutter");
		ImGui::TableNextColumn();
		ImGui::Text("%.02f (%i > %i FPS)", timestampStutter - now, prevFramerate, stutterTarget);
	}

	if (running) {
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Slow");
		ImGui::TableNextColumn();
		ImGui::Text("%.02f (%i > %i FPS)", timestamp - now, prevFramerate, slowTarget);
	}
}
