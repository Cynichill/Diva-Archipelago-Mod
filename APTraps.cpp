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

	std::cout << "[Archipelago] trap_duration: " << trapDuration << " (config: " << config_duration << ")" << std::endl;

	std::string config_iconinterval = data["icon_reroll"].value_or(std::to_string(iconInterval));
	iconInterval = std::clamp(std::stof(config_iconinterval), 0.0f, 60.0f);

	std::cout << "[Archipelago] icon_reroll: " << iconInterval << " (config: " << config_iconinterval << ")" << std::endl;

	std::random_device rd;
	mt.seed(rd());

	reset();
}

int APTraps::reset()
{
	resetIcon();
	setModifier(DIVA_MODIFIERS::None);
	remove(TrapIconInFile.c_str());

	return 0;
}

void APTraps::resetIcon()
{
	if (savedIcon == 39)
		return;

	int restoredIcon = ((savedIcon <= 12 && savedIcon >= 0) ? savedIcon : 4);
	if (getCurrentIcon() != restoredIcon) {
		WRITE_MEMORY(getIconAddress(), uint8_t, (uint8_t)restoredIcon);
		std::cout << "[Archipelago] Icons restored to " << restoredIcon << std::endl;
	}
	savedIcon = 39;
}

bool APTraps::exists(const std::string& in)
{
	return std::filesystem::exists(in.c_str());
}

// Very happening function.
void APTraps::run() 
{
	auto now = *(float*)DivaGameTimer;
	bool sudden_exists = exists(TrapSuddenInFile);
	bool hidden_exists = exists(TrapHiddenInFile);
	bool icon_exists = exists(TrapIconInFile);
	
	if (sudden_exists) {
		std::cout << "[Archipelago] Trap < Sudden" << std::endl;
		setModifier(DIVA_MODIFIERS::Sudden);
		remove(TrapSuddenInFile.c_str());
	}

	if (hidden_exists) {
		std::cout << "[Archipelago] Trap < Hidden" << std::endl;
		setModifier(DIVA_MODIFIERS::Hidden);
		remove(TrapHiddenInFile.c_str());
	}

	if (sudden_exists || hidden_exists)
		timestampModifier = now;

	auto deltaModifier = now - timestampModifier;
	if (getCurrentModifier() > 0 && trapDuration > 0 && deltaModifier >= trapDuration) {
		std::cout << "[Archipelago] Trap > Modifier " << getCurrentModifier() << " expired" << std::endl;
		setModifier(DIVA_MODIFIERS::None);
	}

	if (icon_exists) {
		std::cout << "[Archipelago] Trap < Icon" << std::endl;		
		timestampIconStart = now;
		rollIcon();
		remove(TrapIconInFile.c_str());

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
			std::cout << "[Archipelago] Trap > Icon expired" << std::endl;
			resetIcon();
		}
	}
}

uint64_t APTraps::getGameControlConfig() const
{
	static uint64_t GCC = reinterpret_cast<uint64_t(__fastcall*)(void)>(DivaGameControlConfig)();
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
