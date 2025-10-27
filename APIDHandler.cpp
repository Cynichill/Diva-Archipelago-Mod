#include "APIDHandler.h"
#include <filesystem>
#include <fstream>
#include <iostream>
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
		std::cout << "[Archipelago] New Classics suspected, reload recommended" << std::endl;
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
	//std::cout << "[Archipelago] ID Handler reset" << std::endl;
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
		std::cout << "[Archipelago] Toggle IDs: ";

		while (std::getline(file, buf)) {
			try {
				auto pvID = std::stoi(buf);

				std::cout << buf << " ";

				if (pvID == 0) {
					freeplay = true;
					continue;
				}

				add(std::stoi(buf));
			}
			catch (std::invalid_argument const& ex) {
				std::cout << "(inv: " << buf << ") ";
			}
			catch (std::out_of_range const& ex) {
				std::cout << "(range: " << buf << ") ";
			}
		}

		std::cout << "(freeplay: " << freeplay << ")" << std::endl;
	}

	file.close();
}

void APIDHandler::add(int newID)
{
	if (reload_needed) {
		std::cout << "[Archipelago] Attempted to add " << newID << " but a reload is needed" << std::endl;
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
