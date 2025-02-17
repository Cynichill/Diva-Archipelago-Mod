#pragma once

#include "SubModules/apclientpp/apclient.hpp"
#include "SubModules/apclientpp/apuuid.hpp"

using nlohmann::json;

class CArchipelago {
public:
	BOOL Initialise(std::string URI);
	VOID say(std::string message);
	BOOLEAN isConnected();
	VOID update();
	VOID gameFinished();
	VOID sendDeathLink();
};
