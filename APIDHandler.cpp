#include "APIDHandler.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <Windows.h>

APIDHandler::APIDHandler() {
}

bool APIDHandler::exists() {
	return std::filesystem::exists(SongListFile);
}

bool APIDHandler::checkNC() {
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
	if (reload_needed || "pv_" != line.substr(0, 3) || !exists())
		return true;

	auto shortID = line.substr(3, 4);
	if ("144." == shortID || "700." == shortID || "701." == shortID)
		return true;

	std::cmatch matches;
	static std::regex pattern("^pv_([0-9]+).difficulty.(easy|normal|hard|extreme).length");

	if (!std::regex_search(line.c_str(), matches, pattern))
		return true;

	if (!matches.empty()) {
		bool c = contains(std::stoi(matches[1]));

		return freeplay ? !c : c;
	}

	return false;
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
			if (buf.substr(0, 1) == "-")
				freeplay = true;

			try {
				add(std::stoi(buf));
				std::cout << buf << " ";
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
	reloading = true;
}

void APIDHandler::unlock()
{
	reloading = false;
}
