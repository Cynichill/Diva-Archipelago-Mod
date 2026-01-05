#pragma once
#include <chrono>
#include <filesystem>
#include <random>
#include <stdint.h>
#include <toml++/toml.h>

namespace fs = std::filesystem;

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
		float lastRun = 0.0f; // For delta time against APTraps::DivaGameTimer

		const fs::path LocalPath = fs::current_path();
		const fs::path TrapSuddenInFile = "sudden";
		const fs::path TrapHiddenInFile = "hidden";
		const fs::path TrapIconInFile = "icontrap";

		bool exists(const fs::path& in);
		uint64_t getGameControlConfig();
		uint64_t getIconAddress();
		int getCurrentModifier();

		uint8_t getCurrentIcon();
		void setModifier(int index);
		void rollIcon();
};

