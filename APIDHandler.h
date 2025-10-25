#pragma once
#include <vector>
#include <string>
class APIDHandler
{
	public:
		APIDHandler();

		bool exists();

		bool checkNC();

		// Mostly for New Classics, but improves stability overall.
		// Load "everything" first then require a manual refresh.
		// If true, do not act on toggleIDs.
		bool reload_needed = true;

		// reloading = false
		void unlock();

		// Do not modify while first_run is true. If empty, allow everything.
		// If freeplay is true these will be hidden instead.
		std::vector<int> toggleIDs = { };

		// If true, hide toggleIDs instead of only showing.
		bool freeplay = false;

		bool check(std::string& line);
		void reset();
		void update();

		void add(int songID);
		bool contains(int songID);

	private:
		const std::string SongListFile = "mods/ArchipelagoMod/song_list.txt";

		// Debounce STARTUP/DATA_TEST states.
		bool reloading = false;

		// reloading = true
		void lock();
};

