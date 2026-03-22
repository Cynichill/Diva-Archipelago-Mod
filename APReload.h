#pragma once
#include <toml++/toml.h>

class APReload
{
	public:
		void config(toml::v3::ex::parse_result& data);
		void scan();
		void sleepStartup();

		// Config
		int reloadDelay = 1000; // ms delay to allow state change
		std::string reloadVal = "F7"; // human readable reload key to convert to key code
		int reloadKeyCode = 0x76; // key code of reloadVal

	private:
		void ChangeGameState(int32_t state);
		void ChangeGameSubState(int32_t state, int32_t substate);
};

