#include "APLogger.h"
#include "APTraps.h"
#include "Diva.h"
#include "Helpers.h"
#include "pch.h"
#include <filesystem>
#include <iostream>

APTraps::APTraps() : dist(0, 4)
{
}

void APTraps::config(toml::v3::ex::parse_result& data)
{
	std::string config_duration = data["trap_duration"].value_or(std::to_string(trapDuration));
	trapDuration = std::clamp(std::stof(config_duration), 0.0f, 180.0f);

	APLogger::print("trap_duration: %.02f (config: %s)\n", trapDuration, config_duration.c_str());

	std::string config_iconinterval = data["icon_reroll"].value_or(std::to_string(iconInterval));
	iconInterval = std::clamp(std::stof(config_iconinterval), 0.0f, 60.0f);

	APLogger::print("icon_reroll: %.02f (config: %s)\n", iconInterval, config_iconinterval.c_str());

	std::random_device rd;
	mt.seed(rd());

	reset();
}

int APTraps::reset()
{
	APLogger::print("Traps: reset\n");

	resetIcon();
	setModifier(DIVA_MODIFIERS::None);
	timestampModifier = 0;
	lastRun = 0;

	//fs::remove(LocalPath / TrapIconInFile);

	return 0;
}

void APTraps::resetIcon()
{
	if (savedIcon == 39)
		return;

	int restoredIcon = ((savedIcon <= 12 && savedIcon >= 0) ? savedIcon : 4);
	if (getCurrentIcon() != restoredIcon) {
		WRITE_MEMORY(getIconAddress(), uint8_t, (uint8_t)restoredIcon);
		APLogger::print("Icons restored to %i\n", restoredIcon);
	}
	savedIcon = 39;
}

bool APTraps::exists(const fs::path& in)
{
	return fs::exists(LocalPath / in);
}

// Very happening function.
void APTraps::run()
{
	auto now = *(float*)DivaGameTimer;

	if (now == 0.0f && lastRun > 0) {
		reset();
		return;
	}

	if (now - lastRun < 0.1)
		return;

	lastRun = now;

	bool sudden_exists = exists(TrapSuddenInFile);
	bool hidden_exists = exists(TrapHiddenInFile);
	bool icon_exists = exists(TrapIconInFile);

	if (sudden_exists) {
		APLogger::print("[%6.2f] Trap < Sudden\n", now);
		setModifier(DIVA_MODIFIERS::Sudden);
		fs::remove(LocalPath / TrapSuddenInFile);
	}

	if (hidden_exists) {
		APLogger::print("[%6.2f] Trap < Hidden\n", now);
		setModifier(DIVA_MODIFIERS::Hidden);
		fs::remove(LocalPath / TrapHiddenInFile);
	}

	if (sudden_exists || hidden_exists)
		timestampModifier = now;

	auto deltaModifier = now - timestampModifier;
	if (getCurrentModifier() > 0 && trapDuration > 0 && deltaModifier >= trapDuration) {
		APLogger::print("[%6.2f] Trap > Modifier %i expired\n", now, getCurrentModifier());
		setModifier(DIVA_MODIFIERS::None);
	}

	if (icon_exists) {
		APLogger::print("[%6.2f] Trap < Icon\n", now);
		timestampIconStart = now;
		rollIcon();
		fs::remove(LocalPath / TrapIconInFile);

		if (timestampIconStart == now)
			return;
	}

	if (savedIcon <= 12) {
		float deltaStart = now - timestampIconStart;
		float deltaLast = now - timestampIconLast;
		if (deltaStart < trapDuration) {
			if (iconInterval > 0.0f && deltaLast >= iconInterval) {
				timestampIconLast = now;
				rollIcon();
			}
		}
		else {
			APLogger::print("[%6.2f] Trap > Icon expired\n", now);
			resetIcon();
		}
	}
}

uint64_t APTraps::getGameControlConfig()
{
	uint64_t GCC = reinterpret_cast<uint64_t(__fastcall*)(void)>(DivaGameControlConfig)();
	return GCC;
}

uint64_t APTraps::getIconAddress()
{
	return getGameControlConfig() + 0x28;
}

uint8_t APTraps::getCurrentIcon()
{
	return *(uint8_t*)getIconAddress();
}

int APTraps::getCurrentModifier()
{
	return *(int*)DivaGameModifier;
}

void APTraps::setModifier(int index)
{
	if (index >= 0 && index <= 3) {
		WRITE_MEMORY(DivaGameModifier, uint8_t, (uint8_t)index);
		appliedModifier = index;
	}
}

void APTraps::rollIcon()
{
	int currentIcon = getCurrentIcon();
	int nextIcon = dist(mt);

	if (savedIcon > 12)
		savedIcon = currentIcon;

	while (currentIcon == nextIcon) {
		nextIcon = dist(mt);

		if (nextIcon == 4 && currentIcon == 4)
			nextIcon = savedIcon;
		else if (currentIcon >= 9)
			nextIcon += 9;
		else if (currentIcon >= 5)
			nextIcon += 5;
	}

	WRITE_MEMORY(getIconAddress(), uint8_t, (uint8_t)nextIcon);
}
