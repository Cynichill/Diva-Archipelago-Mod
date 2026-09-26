#pragma once
#include "..\APTraps.h"

/*

Base class for traps.

Progressive Trap: A trap containing multiple tiers of the same effect, like Slow and Stutter.
	The primary effect (Slow) uses the base "running" and "timestamp" variables but has a touchSlow().
	No resetSlow() due to Stutter's short duration.

Combination Trap: A trap containing related effects that may interact, like Sudden and Hidden.
	Sudden and Hidden share the same config option to overlap and need to know/control the state of the other.
	Each has a isX, touchX(), and resetX(). Also a getisX(), but that's for the function hooks.

*/

class Trap {
public:
	Trap() { APTraps::registerTrap(this); };

	const float& trapDuration = APTraps::trapDuration; // Global duration for all traps.
	const float& now = APTraps::getGameTime(); // The in-game timer. Stops when the game is paused.
	const bool& devMode = APTraps::devMode; // Global dev mode toggle.
	std::mt19937& mt = APTraps::mt; // Use for RNG. Will reseed on its own.

	virtual void config(const toml::table& settings); // Apply user configs.
	virtual void save(toml::table& settings); // Save user configs.

	// Reset the entire state of the trap. If a progressive/combo trap, reset all of them.
	virtual void reset();

	// Return Trap::running
	virtual bool isRunning();

	// Prepare the trap, including possibly a partial reset, for the next tick().
	// If a progressive/combo trap, start all of them. Individual traps should have a touchX() and resetX().
	// To support extending, preserve and run timestamps through APTraps::getTrapEndTime().
	virtual void touch();

	// Main trap logic called from in-game hook. Traps should check if expired first.
	virtual void tick();

	virtual void ImGuiConfig(); // Provide controls for user-configurable options. Can also hide some behind a "devMode" check.
	virtual void ImGuiStatus(); // For dev mode, provide trap status such as it's time remaining and state to return to.
	virtual void ImGuiExpose(); // For dev mode, expose any additional controls for testing trap internals.

protected:
	// Progressive traps: the primary one should use this.
	// Combination traps: consider isX bools and maybe using this if *any* are running.
	bool running = false;

	// Progressive traps: the primary one should use this.
	// Combination traps: consider timestampX floats and maybe using this if *any* are running.
	// To support extending, run through APTraps::getTrapEndTime() and maintain it across touch()s.
	float timestamp = 0.0f;
};
