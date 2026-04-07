#pragma once
#include "pch.h"
#include <vector>

namespace APIDHandler
{
	// Mostly for New Classics, but improves stability overall.
	// Load "everything" first then require a manual refresh.
	// If true, do not act on toggleIDs.
	extern bool reload_needed;

	void config(toml::v3::ex::parse_result& data);

	bool checkNC();
	void lock();
	void unlock();
	bool check(std::string& line);
	void reset();

	void ImGuiTab();
}
