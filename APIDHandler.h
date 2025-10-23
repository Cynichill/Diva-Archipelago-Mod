#pragma once
#include <vector>
#include <string>
class APIDHandler
{
	public:
		std::vector<int> enabledIDs = { }; // If empty, allow everything.

		bool check(std::string line);
		void clear();
		void update();

		void add(int songID);
		bool contains(int songID);

	private:
};

