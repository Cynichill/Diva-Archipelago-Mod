#pragma once
#include "trap.h"

namespace TrapHiSpeed
{
	struct Note {
		int type;
		float pos_x;
		float pos_y;
		float origin_x;
		float origin_y;
		float amp;
		int freq;
		bool slide_start;
		bool slide_end;
		char unk[6];
	};

	void installHooks();

	class _TrapHiSpeed : public Trap {
	public:
		_TrapHiSpeed() : Trap() {
			installHooks();
		};

		void config(const toml::table& settings);
		void save(toml::table& settings);
		void resetHiSpeed();
		void resetNoSpeed();
		void resetLane();
		void reset();
		bool isRunningNoSpeed() const;
		bool isRunningLane() const;
		void touchHiSpeed();
		void touchNoSpeed();
		void touchLane();
		void touch();
		void tick();
		void ImGuiConfig();
		void ImGuiStatus();
		void ImGuiExpose();

	private:
		bool isNoSpeed = false;
		float timestampNoSpeed = 0.0f;

		bool isLane = false;
		float timestampLane = 0.0f;
	};

	int getHighSpeedRate();

	extern _TrapHiSpeed trap;
}
