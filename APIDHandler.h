#pragma once
#include "pch.h"
#include <vector>

namespace APIDHandler
{
	// Mostly for New Classics, but improves stability overall.
	// Load "everything" first then require a manual refresh.
	// If true, do not act on toggleIDs.
	extern bool reload_needed;

	// Do not modify while reload_needed is true. If empty, allow everything.
	// If `freeplay` is true these will be hidden instead.
	extern std::vector<int> toggleIDs;

	// If true, hide toggleIDs instead of only showing.
	extern bool freeplay;

	bool checkNC();
	void lock();
	void unlock();
	bool check(std::string& line);
	void reset();
	void update();
	void add(int songID);
	bool contains(int songID);

	// Was way too slow verifying it per pv_db line, so don't do that.
	void cacheExists();
}
