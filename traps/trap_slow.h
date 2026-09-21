#pragma once
#include "trap.h"

class TrapSlow : public Trap {
public:
	TrapSlow() : Trap("Slow") {}

	void config(const toml::table& settings);
	void save(toml::table& settings);
	void reset();
	void touch();
	void touch(bool stutter);
	void tick();
	void resetFramerate();
	void ImGuiConfig();
	void ImGuiStatus();

private:
	void touchStutter();

	int prevFramerate = 0; // 0 = Unlimited by the game, +0x4 for vsync
	int slowTarget = 30; // Target framerate before Stutter or Slow overwrite it. The game applies 0 as "uncapped", then considers vsync at addr+0x4

	bool isStutter = false;
	float timestampStutter = 0.0f;
	int stutterTarget = 10; // FPS that Stutter drops to briefly. Too low can get the trap "stuck" on for longer than intended.
};
