#include "trap_hispeed.h"
#include "../APClient.h"

namespace TrapHiSpeed
{
	void _TrapHiSpeed::config(const toml::table& settings)
	{
		hiSpeedFactor = std::clamp(settings["hispeed_factor"].value_or(hiSpeedFactor), -10.0f, 10.0f);
		APLogger::print("hispeed_factor: %i\n", hiSpeedFactor);
	}

	void _TrapHiSpeed::save(toml::table& settings)
	{
		settings.insert("hispeed_factor", hiSpeedFactor);
	}

	void _TrapHiSpeed::resetHiSpeed()
	{
		Trap::reset();
	}

	void _TrapHiSpeed::resetNoSpeed()
	{
		isNoSpeed = false;
		timestampNoSpeed = 0.0f;
	}

	void _TrapHiSpeed::reset()
	{
		resetHiSpeed();
		resetNoSpeed();
	}

	bool _TrapHiSpeed::isRunningNoSpeed() const
	{
		return isNoSpeed;
	}

	void _TrapHiSpeed::touchHiSpeed()
	{
		resetNoSpeed();

		timestamp = APTraps::getTrapEndTime(timestamp);
		running = true;

		APLogger::print("[%6.2f] Trap < HiSpeed (expires: %.2f)\n", now, timestamp);
	}

	void _TrapHiSpeed::touchNoSpeed()
	{
		resetHiSpeed();

		timestampNoSpeed = APTraps::getTrapEndTime(timestampNoSpeed);
		isNoSpeed = true;

		APLogger::print("[%6.2f] Trap < NoSpeed (expires: %.2f)\n", now, timestampNoSpeed);
	}

	void _TrapHiSpeed::touch()
	{
		touchHiSpeed();
	}

	void _TrapHiSpeed::tick()
	{
		if (!running && !isNoSpeed) return;

		if (isNoSpeed && now >= timestampNoSpeed) {
			APLogger::print("[%6.2f] Trap > NoSpeed expired\n", now);
			//resetNoSpeed();
			touchHiSpeed();
			return;
		}

		if (running && now >= timestamp) {
			APLogger::print("[%6.2f] Trap > HiSpeed expired\n", now);
			//reset();
			touchNoSpeed();
			return;
		}
	}

	int note = 0;
	float testX = 0.0f;
	float testY = 0.0f;

	void _TrapHiSpeed::ImGuiConfig()
	{
		static std::string fmt;
		fmt = std::format("{:.3f} x {} = {:.3f}", hiSpeedFactor, getHighSpeedRate(), hiSpeedFactor * getHighSpeedRate());

		if (ImGui::SliderFloat("HiSpeed Factor", &hiSpeedFactor, 0.5, 5.0, fmt.c_str()))
			hiSpeedFactor = std::clamp(hiSpeedFactor, -10.0f, 10.0f);
		HelpMarker("Fine tune the song's default high speed rate.\nThe second operand is from the latest song played.\n\n0 is equivalent to NoSpeed.\nNegative flips the trajectory.\nTry extremely small numbers close to 0!");
	}

	void _TrapHiSpeed::ImGuiStatus()
	{
		if (running)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("HiSpeed");
			ImGui::TableNextColumn();
			ImGui::Text("%.02f", timestamp - now);
		}

		if (isNoSpeed)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("NoSpeed");
			ImGui::TableNextColumn();
			ImGui::Text("%.02f", timestampNoSpeed - now);
		}
	}

	void _TrapHiSpeed::ImGuiExpose()
	{
		if (ImGui::Button("HiSpeed"))
			touchHiSpeed();

		ImGui::SameLine();

		if (ImGui::Button("NoSpeed"))
			touchNoSpeed();
	}

	float _TrapHiSpeed::getHiSpeedFactor() const
	{
		return hiSpeedFactor;
	}

	HOOK(void, __fastcall, _NoteModifier, 0x14026E8E0, long long *a1, uintptr_t *note, float a3, int a4, long long _5, int a6, long long *a7, long long a8, int noteTotal) {
		int& modifier = *reinterpret_cast<int*>(PvPlayData + 0x2D120); // TODO: Move to APTraps?
		if (!trap.isRunning() && !trap.isRunningNoSpeed() || note == nullptr || modifier == 1) {
			original_NoteModifier(a1, note, a3, a4, _5, a6, a7, a8, noteTotal);
			return;
		}

		// Due to traps being temporary and AP potentially being retry heavy the original note has to be preserved.
		// Functions this one calls out to could skip the backup and restore, but not all props are ready (freq).
		// A copy was done original, but this works off the copy instead of copying back.
		Note _note = *(Note*)note;
		uintptr_t* _note_ptr = reinterpret_cast<uintptr_t*>(&_note);

		if (trap.isRunning()) {
			int rate = getHighSpeedRate();
			float factor = trap.getHiSpeedFactor();

			auto _x = abs(_note.pos_x - _note.origin_x) * rate * factor;
			auto _y = abs(_note.pos_y - _note.origin_y) * rate * factor;

			_note.freq *= max(1, static_cast<int>(rate * factor));
			_note.origin_x = _note.pos_x + (_note.pos_x > _note.origin_x ? _x * -1.0f : _x);
			_note.origin_y = _note.pos_y + (_note.pos_y > _note.origin_y ? _y * -1.0f : _y);
		}
		else if (trap.isRunningNoSpeed()) {
			_note.origin_x = _note.pos_x; // +-1 for a little movement
			_note.origin_y = _note.pos_y;
		}

		original_NoteModifier(a1, _note_ptr, a3, a4, _5, a6, a7, a8, noteTotal);
	}

	void installHooks()
	{
		INSTALL_HOOK(_NoteModifier);
	}

	int getHighSpeedRate()
	{
		static auto getHighSpeedRate = reinterpret_cast<int(__fastcall*)(uintptr_t PvPlayData)>(0x14024b630);
		return getHighSpeedRate(PvPlayData);
	}

	_TrapHiSpeed trap;
}
