#pragma once
#include <chrono>
#include <filesystem>
#include <stdint.h>
#include <string>
#include <toml++/toml.h>

namespace fs = std::filesystem;

class APDeathLink
{
	public:
		APDeathLink();
		void config(toml::v3::ex::parse_result& data);

		bool deathLinked = false; // Who wants to know?

		bool exists(const fs::path& in);
		int touch();

		void reset();
		void check_fail();

		void run();

		// Refresh HP chunks or call reset
		void prog_hp_update();

		void prog_hp_reset();

		int HPnumerator = 1; // Current HP chunks received
		int HPdenominator = 1; // Total HP chunks
		int HPprefloor = 76; // 0-255, default safety HP. used for rolling safety bar.
		int HPprepercent = 30; // 0-100, default safety bar percentage. used for rolling safety bar.
		int HPfloor = 0; // 0-255, highest HP value that triggers death and when to engage
		int HPpercent = 0; // 0-100, percentage of safety bar
		bool HPengaged = false; // True when HPfloor is met
		bool safetyExpired = false; // Easy 60, Norm 40, Hard+ 30s

	private:
		const uint64_t DivaGameHP = 0x00000001412EF564;
		const uint64_t DivaGameTimer = 0x00000001412EE340;
		const uint64_t DivaSafetyWidthPercent = 0x00000001412EF644;

		std::string buf;

		const fs::path LocalPath = fs::current_path();
		const fs::path DeathLinkInFile = "death_link_in"; // Inbound communication file
		const fs::path DeathLinkOutFile = "death_link_out"; // Outbound communication file

		const fs::path HPFile = "hp"; // Progressive HP file
		const fs::path HPFileNext = "hp.txt"; // Progressive HP file with ext for easy editing
		float lastCheckedHP = 0.0f; // HP: For delta time against APDeathLink::DivaGameTimer

		int percent = 100; // Percentage of max HP to lose on receive. "If at or below this, die."
		float safety = 10.0f; // Seconds after receiving a DL to avoid chain reaction DLs.
		float lastDeathLink = 0.0f; // Compared against APDeathLink::safety
};

