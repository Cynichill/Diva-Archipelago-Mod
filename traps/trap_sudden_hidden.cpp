#include "trap_sudden_hidden.h"

namespace TrapSuhidden
{
	void _TrapSuhidden::config(const toml::table& settings)
	{
		overlap = settings["overlap"].value_or(overlap);
		APLogger::print("trap suhidden overlap: %d\n", overlap);
	}

	void _TrapSuhidden::save(toml::table& settings)
	{
		settings.insert("overlap", overlap);
	}

	void _TrapSuhidden::reset()
	{
		Trap::reset();

		resetSudden();
		resetHidden();
	}

	void _TrapSuhidden::resetSudden()
	{
		timestampSudden = 0.0f;
		isSudden = false;
	}

	void _TrapSuhidden::resetHidden()
	{
		timestampHidden = 0.0f;
		isHidden = false;
	}

	void _TrapSuhidden::touch()
	{
		// Suhidden is finally its own trap!
		touchSudden(true);
		touchHidden(true);
	}

	void _TrapSuhidden::touchSudden()
	{
		touchSudden(false);
	}

	void _TrapSuhidden::touchSudden(bool force_overlap)
	{
		timestampSudden = APTraps::getTrapEndTime(timestampSudden);

		APLogger::print("[%6.2f] Trap < Sudden (expires: %.2f)\n", now, timestampSudden);
		isSudden = true;

		bool _overlap = force_overlap || overlap;

		if (!_overlap && isHidden) {
			APLogger::print("[%6.2f] Trap < Hidden -> Sudden (expires: %.2f)\n", now, timestampSudden);
			resetHidden();
		}
	}

	void _TrapSuhidden::touchHidden()
	{
		touchHidden(false);
	}

	void _TrapSuhidden::touchHidden(bool force_overlap)
	{
		timestampHidden = APTraps::getTrapEndTime(timestampHidden);

		APLogger::print("[%6.2f] Trap < Hidden (expires: %.2f)\n", now, timestampHidden);
		isHidden = true;

		bool _overlap = force_overlap || overlap;

		if (!_overlap && isSudden) {
			APLogger::print("[%6.2f] Trap < Sudden -> Hidden (expires: %.2f)\n", now, timestampHidden);
			resetSudden();
		}
	}

	bool _TrapSuhidden::getSudden() const
	{
		return isSudden;
	}

	bool _TrapSuhidden::getHidden() const
	{
		return isHidden;
	}

	void _TrapSuhidden::tick()
	{
		if (isSudden && now >= timestampSudden) {
			APLogger::print("[%6.2f] Trap > Sudden expired\n", now);
			resetSudden();
		}

		if (isHidden && now >= timestampHidden) {
			APLogger::print("[%6.2f] Trap > Hidden expired\n", now);
			resetHidden();
		}
	}

	void _TrapSuhidden::ImGuiConfig()
	{
		ImGui::Checkbox("Allow Sudden and Hidden to overlap", &overlap);
	}

	void _TrapSuhidden::ImGuiStatus()
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
	}

	void _TrapSuhidden::ImGuiExpose()
	{
		if (ImGui::Button("Sudden"))
			touchSudden(ImGui::GetIO().KeyShift);
		HelpMarker("Shift+Click: Force overlap");

		ImGui::SameLine();

		if (ImGui::Button("Hidden"))
			touchHidden(ImGui::GetIO().KeyShift);
		HelpMarker("Shift+Click: Force overlap");
	}

	// 0x14024B720
	void* ModifierSudden = sigScan("\x83\xb9\x20\xd1\x02\x00\x03\x0f\x94\xc0\xc3", "xxxxxxxxxxx");

	// 0x14024B730
	void* ModifierHidden = sigScan("\x83\xb9\x20\xd1\x02\x00\x02\x0f\x94\xc0\xc3", "xxxxxxxxxxx");

	HOOK(bool, __fastcall, _ModifierSudden, ModifierSudden, long long a1) {
		return TrapSuhidden::trap.getSudden() || original_ModifierSudden(a1);
	}

	HOOK(bool, __fastcall, _ModifierHidden, ModifierHidden, long long a1) {
		return TrapSuhidden::trap.getHidden() || original_ModifierHidden(a1);
	}

	void installHooks()
	{
		INSTALL_HOOK(_ModifierSudden);
		INSTALL_HOOK(_ModifierHidden);
	}

	_TrapSuhidden trap;
}
