#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class APIDHandler
{
	public:
		APIDHandler();

		bool checkNC();

		// Mostly for New Classics, but improves stability overall.
		// Load "everything" first then require a manual refresh.
		// If true, do not act on toggleIDs.
		bool reload_needed = true;

		// reloading = false
		void unlock();

		// Do not modify while reload_needed is true. If empty, allow everything.
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
		const fs::path LocalPath = fs::current_path();
		const fs::path SongListFile = "song_list.txt";

		bool exists = false;
		// Was way too slow verifying it per pv_db line, so don't do that.
		void cacheExists();

		// Debounce STARTUP/DATA_TEST states.
		bool reloading = false;

		// reloading = true
		void lock();
};

