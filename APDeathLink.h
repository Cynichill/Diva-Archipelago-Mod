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

		bool exists();
		int touch();

		void reset();
		void check_fail();
		void run();

	private:
		const uint64_t DivaGameHP = 0x00000001412EF564;
		const uint64_t DivaGameTimer = 0x00000001412EE340;

		const fs::path LocalPath = fs::current_path();
		const fs::path DeathLinkInFile = "death_link_in"; // Inbound communication file
		const fs::path DeathLinkOutFile = "death_link_out"; // Outbound communication file

		int percent = 100; // Percentage of max HP to lose on receive. "If at or below this, die."
		float safety = 10.0f; // Seconds after receiving a DL to avoid chain reaction DLs.
		float lastDeathLink = 0.0f; // Compared against APDeathLink::safety
};

