#include "APIDHandler.h"
#include "APLogger.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <Windows.h>

APIDHandler::APIDHandler() {
}

void APIDHandler::cacheExists()
{
	exists = std::filesystem::exists(SongListFile);
}

bool APIDHandler::checkNC()
{
	if (!reload_needed)
		return false;

	HMODULE hModule = GetModuleHandle(L"NewClassics.dll");

	if (hModule != NULL) {
		APLogger::print("IDH: New Classics suspected, reload recommended\n");
		return true;
	}

	reload_needed = false;
	return false;
}

bool APIDHandler::check(std::string& line)
{
	if (reload_needed || !exists || line.find(".difficulty") == std::string::npos || line.rfind(".length") == std::string::npos)
		return true;

	size_t start = line.find_first_of("_");
	auto pvID = line.substr(start + 1, line.find_first_of(".") - start - 1);

	// Always enabled to prevent softlocks or crashing.
	if ("144" == pvID || "700" == pvID || "701" == pvID)
		return true;

	bool c = contains(std::stoi(pvID));

	return freeplay ? !c : c;
}

void APIDHandler::reset()
{
	//APLogger::print("IDHandler reset\n");
	unlock();
	freeplay = false;
	toggleIDs.clear();
}

void APIDHandler::update()
{
	if (reloading || checkNC())
		return;

	reset();
	lock();

	std::string buf;
	std::ifstream file(SongListFile);

	if (file.is_open()) {
		std::stringstream toggled;

		while (std::getline(file, buf)) {
			try {
				auto pvID = std::stoi(buf);

				if (pvID == 0) {
					freeplay = true;
					continue;
				}

				add(pvID);
				toggled << buf << " ";
			}
			catch (std::invalid_argument const& ex) {
				APLogger::print("IDH > %s : %s\n", ex.what(), buf);
			}
			catch (std::out_of_range const& ex) {
				APLogger::print("IDH > %s : %s\n", ex.what(), buf);
			}
		}

		APLogger::print("IDH > Toggle IDs %s(freeplay: %i)\n", toggled.str().c_str(), freeplay);
	}

	file.close();
}

void APIDHandler::add(int newID)
{
	if (reload_needed) {
		APLogger::print("IDH > Attempted to add %i but a reload is needed\n", newID);
		return;
	}

	newID = abs(newID);

	if (!contains(newID))
		toggleIDs.push_back(newID);
}

bool APIDHandler::contains(int songID)
{
	// Potentially very slow as the song list grows.
	return std::find(toggleIDs.begin(), toggleIDs.end(), songID) != toggleIDs.end();
}

void APIDHandler::lock()
{
	cacheExists();
	reloading = true;
}

void APIDHandler::unlock()
{
	cacheExists();
	reloading = false;
}
