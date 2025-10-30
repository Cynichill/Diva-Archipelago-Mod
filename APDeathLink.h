#pragma once
#include <chrono>
#include <stdint.h>
#include <string>
#include <toml++/toml.h>

class APDeathLink
{
	public:
		APDeathLink();
		void config(toml::v3::ex::parse_result& data);

		bool deathLinked = false; // Who wants to know?

		bool exists();
		int touch();

		void reset();
		void fail();
		void run();

	private:
		const uint64_t DivaGameHP = 0x00000001412EF564;
		const uint64_t DivaGameTimer = 0x00000001412EE340;

		// Inbound communication file
		const std::string DeathLinkInFile = "mods/ArchipelagoMod/death_link_in";

		// Outbound communication file
		const std::string DeathLinkOutFile = "mods/ArchipelagoMod/death_link_out";

		int percent = 100; // Percentage of max HP to lose on receive. "If at or below this, die."
		float safety = 10.0f; // Seconds after receiving a DL to avoid chain reaction DLs.
		float lastDeathLink = 0.0f; // Compared against APDeathLink::safety
};

