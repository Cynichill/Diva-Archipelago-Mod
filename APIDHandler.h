#pragma once
#include "pch.h"
#include <vector>

struct TrackerItem
{
	int64_t songID = 0;
	int receivedIndex = 0;
	int checksAvailable = 0;
	std::string name = "";
};

namespace APIDHandler
{
	extern int totalLocs;

	void config(const toml::table& settings);
	void save(toml::table& settings);

	void lock();
	void unlock();
	bool check(const std::string& line);
	void reset();

	void slowReleaseRun();

	// Queue a sort the next time the Tracker table is visible.
	// The Tracker (items) will still update in the background, but unsorted.
	void queueTrackerSort();

	void sortTrackerItems(const ImGuiTableSortSpecs* sort_specs);

	void refreshTracker();
	void ImGuiTab();
}
