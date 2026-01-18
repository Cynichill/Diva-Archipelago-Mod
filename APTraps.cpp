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

	suhidden = data["suhidden"].value_or(false);
	APLogger::print("suhidden: %d\n", suhidden);

	std::random_device rd;
	mt.seed(rd());

	reset();
}

int APTraps::reset()
{
	APLogger::print("Traps: reset\n");

	resetIcon();
	timestampSudden = 0.0f;
	timestampHidden = 0.0f;
	isHidden = false;
	isSudden = false;
	lastRun = 0.0f;

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
		APLogger::print("Icons restored to %d\n", restoredIcon);
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

	if (now == 0.0f && lastRun > 0.0f) {
		reset();
		return;
	}

	if (now - lastRun < 0.1f)
		return;

	lastRun = now;

	bool icon_exists = exists(TrapIconInFile);

	if (exists(TrapSuddenInFile)) {
		APLogger::print("[%6.2f] Trap < Sudden\n", now);
		fs::remove(LocalPath / TrapSuddenInFile);
		timestampSudden = now;
		isSudden = true;

		if (!suhidden && isHidden) {
			APLogger::print("[%6.2f] Trap < Hidden -> Sudden\n", now);
			timestampHidden = 0.0f;
			isHidden = false;
		}
	}
	else if (isSudden) {
		auto deltaSudden = now - timestampSudden;
		if (trapDuration > 0.0f && deltaSudden >= trapDuration) {
			APLogger::print("[%6.2f] Trap > Sudden expired\n", now);
			timestampSudden = 0.0f;
			isSudden = false;
		}
	}

	if (exists(TrapHiddenInFile)) {
		APLogger::print("[%6.2f] Trap < Hidden\n", now);
		fs::remove(LocalPath / TrapHiddenInFile);
		timestampHidden = now;
		isHidden = true;

		if (!suhidden && isSudden) {
			APLogger::print("[%6.2f] Trap < Sudden -> Hidden\n", now);
			timestampSudden = 0.0f;
			isSudden = false;
		}
	}
	else if (isHidden) {
		auto deltaHidden = now - timestampHidden;
		if (trapDuration > 0.0f && deltaHidden >= trapDuration) {
			APLogger::print("[%6.2f] Trap > Hidden expired\n", now);
			timestampHidden = 0.0f;
			isHidden = false;
		}
	}

	if (icon_exists) {
		APLogger::print("[%6.2f] Trap < Icon\n", now);
		fs::remove(LocalPath / TrapIconInFile);
		timestampIconStart = now;
		rollIcon();

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
		else if (trapDuration > 0.0f) {
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
