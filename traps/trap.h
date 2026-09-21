#pragma once
#include "..\APTraps.h"

class Trap {
public:
	std::string name = "Trap"; // Human-readable trap name

	Trap(const std::string& name) : name(name) {}

	const float& trapDuration = APTraps::trapDuration;
	const float& now = APTraps::getGameTime();
	const bool& devMode = APTraps::devMode;
	std::mt19937& mt = APTraps::mt; // Use for RNG. Will reseed on its own.

	virtual void config(const toml::table& settings); // Apply user configs.
	virtual void save(toml::table& settings); // Save user configs.

	virtual void reset(); // Reset generic state of the trap (running, timestamp)
	virtual void touch(); // Start the trap
	virtual void tick(); // Run in trap loop

	virtual void ImGuiConfig(); // Provide config options
	virtual void ImGuiStatus(); // Provide config status (usually for dev mode)

protected:
	bool running = false; // ...
	float timestamp = 0.0f; // When this trap expires compared to game time.
};
