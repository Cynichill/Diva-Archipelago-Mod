#pragma once
#include <chrono>
#include <random>
#include <stdint.h>
#include <toml++/toml.h>


class APTraps
{
	public:
		APTraps();
		void config(toml::v3::ex::parse_result& data);

		int reset();
		void resetIcon();
		void run();

		uint8_t savedIcon = 39;
		uint8_t appliedModifier = 0;

	private:
		const uint64_t DivaGameControlConfig = 0x00000001401D6520;
		const uint64_t DivaGameModifier = 0x00000001412EF450;
		const uint64_t DivaGameTimer = 0x00000001412EE340;

		std::mt19937 mt;
		std::uniform_int_distribution<int> dist;

		float trapDuration = 15.0f;
		float iconInterval = 60.0f;
		float timestampModifier = 0.0f;
		float timestampIconStart = 0.0f;
		float timestampIconLast = 0.0f;

		const std::string TrapSuddenInFile = "mods/ArchipelagoMod/sudden";
		const std::string TrapHiddenInFile = "mods/ArchipelagoMod/hidden";
		const std::string TrapIconInFile = "mods/ArchipelagoMod/icontrap";

		bool exists(std::string in);
		uint64_t getGameControlConfig() const;
		uint64_t getIconAddress();
		int getCurrentModifier();

		uint8_t getCurrentIcon();
		void setModifier(int index);
		void rollIcon();
};

