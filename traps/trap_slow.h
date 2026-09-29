#pragma once
#include "trap.h"

namespace TrapSlow
{
	class _TrapSlow : public Trap
	{
	public:
		void config(const toml::table& settings);
		void save(toml::table& settings);
		void reset();
		void touch();
		void touchSlow();
		void touchStutter();
		void tick();
		void resetFramerate();
		void ImGuiConfig();
		void ImGuiStatus();
		void ImGuiExpose();

	private:
		int prevFramerate = 0; // 0 = Unlimited by the game, +0x4 for vsync
		int slowTarget = 30; // Target framerate before Stutter or Slow overwrite it. The game applies 0 as "uncapped", then considers vsync at addr+0x4

		bool isStutter = false;
		float timestampStutter = 0.0f;
		int stutterTarget = 10; // FPS that Stutter drops to briefly. Too low can get the trap "stuck" on for longer than intended.
	};

	extern _TrapSlow trap;
}
