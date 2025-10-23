#include "APIDHandler.h"
#include <regex>
#include <fstream>
#include <iostream>

bool APIDHandler::check(std::string line)
{
	if ("pv_" != line.substr(0, 3) || enabledIDs.empty())
		return true;

	std::cmatch matches;
	static std::regex pattern("^pv_([0-9]+).difficulty.(easy|normal|hard|extreme).length");

	if (!std::regex_search(line.c_str(), matches, pattern))
		return true;

	if (!matches.empty())
		return contains(std::stoi(matches[1]));

	return false;
}

void APIDHandler::clear()
{
	enabledIDs.clear();
}

void APIDHandler::update()
{
	std::string buf;
	std::ifstream file("mods/ArchipelagoMod/enabled.txt");
	if (file.is_open()) {
		enabledIDs = { 144, 700 };
		std::cout << "[Archipelago] Enabling IDs: ";
		while (std::getline(file, buf)) {
			if (!contains(std::stoi(buf))) {
				std::cout << buf << " ";
				enabledIDs.push_back(std::stoi(buf));
			}
		}
		std::cout << std::endl;
		file.close();
	}
}

void APIDHandler::add(int newID)
{
	if (!contains(newID))
		enabledIDs.push_back(newID);
}

bool APIDHandler::contains(int songID)
{
	// Potentially very slow as the song list grows.
	return std::find(enabledIDs.begin(), enabledIDs.end(), songID) != enabledIDs.end();
}
