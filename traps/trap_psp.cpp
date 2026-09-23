#include "..\APTraps.h"
#include "trap_psp.h"

// Crashes if called before rendering. Check APGUI::firstFrame.
auto resize = reinterpret_cast<void(__fastcall*)(int width, int height, bool p3, bool mute)>(0x1402B7CD0);

// Does not crash. Called by above anyway.
//auto resizeB = reinterpret_cast<void(__fastcall*)(void* p1, int width, int height, void* p4)>(0x1402C27E0);

namespace TrapPSP
{
	void _TrapPSP::config(const toml::table& settings)
	{
		pspHeight = std::clamp(settings["psp_height"].value_or(pspHeight), 45, 544);
		APLogger::print("trap psp_height: %i\n", pspHeight);
	}

	void _TrapPSP::save(toml::table& settings)
	{
		settings.insert("psp_height", pspHeight);
	}

	void _TrapPSP::resetRes()
	{
		if (APGUI::firstFrame || APGUI::g_hWnd == nullptr)
			return;

		RECT rect;
		if (GetClientRect(APGUI::g_hWnd, &rect))
			resize(rect.right, rect.bottom, false, false);
		//resizeB(nullptr, rect.right, rect.bottom, nullptr);
	}

	void _TrapPSP::reset()
	{
		Trap::reset();

		resetRes();
	}

	void _TrapPSP::touch()
	{
		if (!running)
			resetRes();

		running = true;
		timestamp = APTraps::getTrapEndTime(timestamp);

		APLogger::print("[%6.2f] Trap < PSP (expires: %.2f)\n", now, timestamp);
	}

	void _TrapPSP::tick()
	{
		if (!running) return;

		if (now >= timestamp) {
			APLogger::print("[%6.2f] Trap > PSP expired\n", now);
			reset();
			return;
		}

		if (APGUI::isInGame()) {
			if (res.height() == pspHeight) return;

			ImGui::SetWindowFocus(nullptr); // The client is going to be unusable anyway.

			RECT rect;
			GetClientRect(APGUI::g_hWnd, &rect);


			int width = pspHeight % 272 == 0 ? 480 * pspHeight / 272 : rect.right * pspHeight / rect.bottom;
			double ratio = (double)width/ (double)rect.right;
			int height = (int)((double)rect.bottom * ratio);

			resize(width, height, false, false);
			//resizeB(nullptr, width, height, nullptr);
		}
		else {
			resetRes();
		}
	}

	void _TrapPSP::resized()
	{
	}

	void _TrapPSP::ImGuiConfig()
	{
		std::string res = std::format("{}x{}", pspHeight % 272 == 0 ? 480 * pspHeight / 272 : pspHeight * 16 / 9, pspHeight);
		if (ImGui::SliderInt("PSP resolution", &pspHeight, 90, 272, res.c_str()))
			pspHeight = std::clamp(pspHeight, 45, 544);
		HelpMarker("Resolution for the PSP Trap.\nBlurry? Try a display mode other than \"Fullscreen\".");
	}

	void _TrapPSP::ImGuiStatus()
	{
		if (!running) return;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("PSP");
		ImGui::TableNextColumn();
		ImGui::Text("%.02f %i x %i", timestamp - now, res.width(), res.height());
	}

	void _TrapPSP::ImGuiExpose()
	{
		if (ImGui::Button("PSP"))
			touch();
	}

	_TrapPSP trap;
}
