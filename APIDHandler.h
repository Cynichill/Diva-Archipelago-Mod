#pragma once
#include <vector>
#include <string>
class APIDHandler
{
	public:
		// Mostly for New Classics, but improves stability overall.
		// Load "everything" first then require a manual refresh.
		// If true, do not act on enabledIDs.
		bool first_run = true;

		// Do not modify while first_run is true. If empty, allow everything.
		std::vector<int> enabledIDs = { }; 

		bool check(std::string line);
		void clear();
		void update();

		void add(int songID);
		bool contains(int songID);

	private:
};

