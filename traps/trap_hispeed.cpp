#include "trap_hispeed.h"

namespace TrapHiSpeed
{
	const char* lanePos[4] = { "Left", "Right", "Top", "Bottom" };

	// Config stored on the NS instead of the class
	float hiSpeedFactor = 1.0f; // Extra mult on base high speed rate
	int lanePosition = 0;
	int laneAmplitude = 1;
	bool laneNormDistance = true;

	void _TrapHiSpeed::config(const toml::table& settings)
	{
		hiSpeedFactor = std::clamp(settings["hispeed_factor"].value_or(hiSpeedFactor), -10.0f, 10.0f);
		APLogger::print("hispeed_factor: %i\n", hiSpeedFactor);

		lanePosition = std::clamp(settings["lane_position"].value_or(lanePosition), 0, 4);
		APLogger::print("lane_position: %i\n", lanePosition);

		laneAmplitude = std::clamp(settings["lane_amplitude"].value_or(laneAmplitude), -5, 5);
		APLogger::print("lane_amplitude: %i\n", laneAmplitude);

		laneNormDistance = settings["lane_normalize"].value_or(laneNormDistance);
		APLogger::print("lane_normalize: %i\n", laneNormDistance);
	}

	void _TrapHiSpeed::save(toml::table& settings)
	{
		//toml::table local;
		settings.insert("hispeed_factor", hiSpeedFactor);
		settings.insert("lane_position", lanePosition);
		settings.insert("lane_amplitude", laneAmplitude);
		settings.insert("lane_normalize", laneNormDistance);

		//settings.insert("hispeed", local);
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

	void _TrapHiSpeed::resetLane()
	{
		isLane = false;
		timestampLane = 0.0f;
	}

	void _TrapHiSpeed::reset()
	{
		resetHiSpeed();
		resetNoSpeed();
		resetLane();
	}

	bool _TrapHiSpeed::isRunningNoSpeed() const
	{
		return isNoSpeed;
	}

	bool _TrapHiSpeed::isRunningLane() const
	{
		return isLane;
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

	void _TrapHiSpeed::touchLane()
	{
		timestampLane = APTraps::getTrapEndTime(timestampLane);
		isLane = true;

		APLogger::print("[%6.2f] Trap < Lane (expires: %.2f)\n", now, timestampLane);
	}

	void _TrapHiSpeed::touch()
	{
		touchHiSpeed();
	}

	void _TrapHiSpeed::tick()
	{
		if (!running && !isNoSpeed && !isLane) return;

		if (isLane && now >= timestampLane) {
			APLogger::print("[%6.2f] Trap > Lane expired\n", now);
			resetLane();
			return;
		}

		if (isNoSpeed && now >= timestampNoSpeed) {
			APLogger::print("[%6.2f] Trap > NoSpeed expired\n", now);
			resetNoSpeed();
			return;
		}

		if (running && now >= timestamp) {
			APLogger::print("[%6.2f] Trap > HiSpeed expired\n", now);
			resetHiSpeed();
			return;
		}
	}

	void _TrapHiSpeed::ImGuiConfig()
	{
		if (ImGui::CollapsingHeader("HiSpeed / NoSpeed / Lane")) {
			static std::string fmt;
			fmt = std::format("{:.3f} x {} = {:.3f}", hiSpeedFactor, getHighSpeedRate(), hiSpeedFactor * getHighSpeedRate());

			if (ImGui::SliderFloat("HiSpeed Factor", &hiSpeedFactor, 0.5, 5.0, fmt.c_str()))
				hiSpeedFactor = std::clamp(hiSpeedFactor, -10.0f, 10.0f);
			HelpMarker("Fine tune the song's default high speed rate.\nThe second operand is from the current song played.\n\n0 is equivalent to NoSpeed.\nNegative flips the trajectory.\nTry extremely small numbers close to 0!");

			ImGui::Combo("Lane edge", &lanePosition, lanePos, IM_COUNTOF(lanePos));

			if (ImGui::SliderInt("Amplitude multiplier", &laneAmplitude, -3, 3))
				laneAmplitude = std::clamp(laneAmplitude, -5, 5);
			HelpMarker("Multiply note amplitudes (waviness) by this amount.\n\n0 would be a straight line.\n1 would be the original value.");

			ImGui::Checkbox("Lane: Normalize distances", &laneNormDistance);
			HelpMarker("Enabled: All notes will travel the same distance.\nMost notes should be unaffected by this.\n\nDisabled: Notes with greater distances can fly in faster than preceding notes. This is closer to the original chart's intent, but not completely accurate.");
		}
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

		if (isLane)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Lane");
			ImGui::TableNextColumn();
			ImGui::Text("%.02f", timestampLane - now);
		}
	}

	void _TrapHiSpeed::ImGuiExpose()
	{
		if (ImGui::Button("HiSpeed"))
			touchHiSpeed();

		ImGui::SameLine();

		if (ImGui::Button("NoSpeed"))
			touchNoSpeed();

		ImGui::SameLine();

		if (ImGui::Button("Lane"))
			touchLane();
	}

	HOOK(void, __fastcall, _NoteModifier, 0x14026E8E0, uintptr_t *a1, uintptr_t *note, float flying_time, int a4, int a5, int a6, long long *a7, long long a8, int noteTotal) {
		int& modifier = *reinterpret_cast<int*>(PvPlayData + 0x2D120); // TODO: Move to APTraps?
		if (modifier == 1 || note == nullptr || !trap.isRunningLane() && !trap.isRunning() && !trap.isRunningNoSpeed()) {
			original_NoteModifier(a1, note, flying_time, a4, a5, a6, a7, a8, noteTotal);
			return;
		}

		// Due to traps being temporary and AP potentially being retry heavy the original note has to be preserved.
		// Functions this one calls out to could skip the backup and restore, but not all props are ready (freq).
		// A copy was done originally, but this works off the copy instead of copying back.
		Note _note = *(Note*)note;
		uintptr_t* _note_ptr = reinterpret_cast<uintptr_t*>(&_note);

		if (trap.isRunningLane()) {
			// TODO: flying_time is RIGHT THERE!

			// Surely this is available somewhere else already to track steps through a multi note
			static int index = 0;
			static int last_type = -1;
			index += 1;

			static int mult_table[4] = { 1, 4, 3, 2 };
			static int mult = 0;

			float offset_x = 0.0f;
			float offset_y = 0.0f;

			if (_note.type >= 0 && _note.type <= 7)
			{
				mult = mult_table[_note.type % 4];
			}
			else if (_note.type >= 18 && _note.type <= 21) {
				mult = mult_table[(_note.type - 18) % 4];
			}
			else if (_note.type >= 29 && _note.type <= 36) {
				mult = mult_table[(_note.type - 29) % 4];
			}
			else if (_note.type == 12 || _note.type == 15 || _note.type == 23) { // Left chain/slide
				mult = 0;

				if (noteTotal == 2 && _note.type == last_type && index > 1) {
					mult = -1;
					offset_x = -24.0f;
				}
			}
			else if (_note.type == 13 || _note.type == 16 || _note.type == 24) { // Right chain/slide
				mult = 5;

				if (noteTotal == 2 && _note.type == last_type && index > 1) {
					mult = 6;
					offset_x = -24.0f;
				}
			}
			else if (_note.type >= 25 && _note.type <= 28) { // Rush notes
				mult = mult_table[(_note.type - 25) % 4];
			}
			else if (_note.type == 37 || _note.type == 39 || _note.type == 40 || _note.type == 41 || _note.type == 42) { // Star, Double, Chance, Link, Link End
				static bool flipped = false;
				static bool step = false;

				mult = flipped ? 0 : 5;

				if (_note.type == 41 || _note.type == 42) {
					// Improve reading links

					step = !step;

					if (!step) {
						offset_x = -24.0f;
						mult = flipped ? -1 : 6;
					}

					if (_note.type == 42) {
						step = false;
						flipped = !flipped;
					}
				}
			}

			auto distance = laneNormDistance ? 0 : hypot(abs(_note.pos_x - _note.origin_x), abs(_note.pos_y - _note.origin_y));
			_note.amp *= laneAmplitude;
			int laneIndex = lanePosition;

			if (laneIndex == 0 || laneIndex == 1) {
				_note.pos_x = laneIndex == 0 ? 96.0f + offset_x : 384.0f - offset_x;
				//_note.origin_x = laneIndex == 0 ? 480.0f + offset_x : -12.0f;
				_note.origin_x = laneIndex == 0 ? 12.0f + max(480.0f, distance) + offset_x : 384.0f - 12.0f - max(384.0f, distance) - offset_x;

				_note.pos_y = 77.0f + (24.0f * mult) + offset_y;
				_note.origin_y = _note.pos_y + offset_y;
			}
			else if (laneIndex == 2 || laneIndex == 3) {
				_note.pos_x = 168.0f + (24.0f * mult);
				_note.origin_x = _note.pos_x;

				_note.pos_y = laneIndex == 3 ? 270.0f - 48.0f : 48.0f;
				_note.origin_y = laneIndex == 3 ? -36.0f : 270.0f + 36.0f;
			}

			last_type = _note.type;

			if (index == noteTotal) {
				index = 0;

				if (_note.type != 41)
					last_type = -1;
			}
		}

		if (trap.isRunning()) {
			int rate = getHighSpeedRate();
			float factor = hiSpeedFactor;

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

		original_NoteModifier(a1, _note_ptr, flying_time, a4, a5, a6, a7, a8, noteTotal);
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
