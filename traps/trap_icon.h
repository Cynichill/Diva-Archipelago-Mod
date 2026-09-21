#pragma once
#include "trap.h"

class TrapIcon : public Trap {
public:
	TrapIcon() : Trap("Icon") {}

	void config(const toml::table& settings);
	void save(toml::table& settings);
	void reset();
	void touch();
	void tick();
	uintptr_t getGameControlConfig();
	uintptr_t getIconAddress();
	int& getCurrentIcon();
	void resetIcon();
	void rollIcon();
	void ImGuiConfig();
	void ImGuiStatus();

private:
	const uintptr_t DivaGameControlConfig = 0x1401D6520;
	const uintptr_t PvControllerGlyphBase = 0x141133D30; // Copy of GCC Icon on load (0-12), original caller returns base glyph (0-2).

	int savedIcon = 39; // If randomizeGlyphs: also used to restore glyphs
	float rerollInterval = 60.0f;
	float timestampRollNext = 0.0f;
	bool alternateArrows = true; // By expanding the range to all glyphs but not changing the glyph address, expose other colored arrows.
	bool randomizeGlyphs = false;
};
