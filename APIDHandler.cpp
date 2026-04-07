#include "APClient.h"
#include "APHints.h"
#include "APIDHandler.h"
#include "APReload.h"

namespace APIDHandler
{
	// Internal
	bool exists = false;
	bool freeplay = false;
	bool autoremove = true;
	bool reload_needed = true;
	bool reloading = false;

	auto &CheckedLocations = APClient::CheckedLocations;
	auto &seedIDs = APClient::seedIDs;
	auto &recvIDs = APClient::recvIDs;
	auto &missingIDs = APClient::missingIDs;
	auto &item_ap_id_to_name = APClient::item_ap_id_to_name;
	int availableLocs = 0; // Calculated on reload

	auto &HintedIDs = APHints::HintedIDs;

	bool checkNC()
	{
		if (!reload_needed)
			return false;

		HMODULE hModule = GetModuleHandle(L"NewClassics.dll");

		if (hModule != NULL) {
			APLogger::print("IDH: New Classics suspected, reload recommended\n");
			return true;
		}

		reload_needed = false;
		return false;
	}

	bool check(std::string& line)
	{
		if (reload_needed /*|| AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated*/ || missingIDs.size() == 0 || line.find("pv_") != 0)
			return true;

		size_t diff_pos = line.find(".difficulty.");
		size_t len_pos = line.rfind(".length");

		if (diff_pos == std::string::npos || len_pos == std::string::npos)
			return true;

		// Naively restrict to pv_#.difficulty.easy.length (difficulty and length already confirmed in the string)
		if (3 != std::count(line.begin(), line.end(), '.'))
			return true;

		size_t start = line.find_first_of("_");
		int pvID = std::stoi(line.substr(start + 1, line.find_first_of(".") - start - 1));

		// Always enabled to prevent softlocks or crashing.
		if (144 == pvID || 700 == pvID || 701 == pvID)
			return true;

		auto begin = freeplay ? missingIDs.begin() : recvIDs.begin();
		auto end = freeplay ? missingIDs.end() : recvIDs.end();
		auto contains = std::find(begin, end, pvID) != end;

		if (!freeplay && contains && autoremove)
		{
			for (const auto& songID : recvIDs) {
				auto loc1checked = std::find(CheckedLocations.begin(), CheckedLocations.end(), pvID * 10) != CheckedLocations.end();
				auto loc2checked = std::find(CheckedLocations.begin(), CheckedLocations.end(), (pvID * 10) + 1) != CheckedLocations.end();
				if (loc1checked && loc2checked)
					return false;
			}
		}

		return freeplay ? !contains : contains;
	}

	void reset()
	{
		//APLogger::print("IDHandler reset\n");
		freeplay = false;
		unlock();
	}

	void lock()
	{
		reloading = true;
	}

	void unlock()
	{
		reloading = false;
	}

	void ImGuiTab()
	{
		if (ImGui::BeginTabItem("Tracker")) {
			ImGui::Text("Songs: %d/%d |", recvIDs.size(), seedIDs.size() - 1);

			ImGui::SameLine();
			int totalLocs = (seedIDs.size() - 1) * 2;
			// APCpp's check callback runs multiple times around goaling?
			ImGui::Text("Locs: %d/%d |", min(CheckedLocations.size(), totalLocs), totalLocs);

			ImGui::SameLine();
			ImGui::Text("In logic: %d |", availableLocs);

			ImGui::SameLine();
			ImGui::Text("Leeks: %d/%d", APClient::leekHave, APClient::leekNeed);

			if (ImGui::Checkbox("Freeplay", &freeplay))
				APReload::run();
			ImGui::SameLine();
			HelpMarker("The entire song list will be available except for songs that have not been received yet.");

			if (ImGui::Checkbox("Remove songs with no checks from song list", &autoremove))
				APReload::run();
			ImGui::SameLine();
			HelpMarker("When not in Freeplay, the song list will only show songs that have checks.");

			if (ImGui::BeginChild("tableTrackerContainer", ImVec2(0, 300))) {
				if (ImGui::BeginTable("tableTracker", 2,
					ImGuiTableFlags_BordersInner | ImGuiTableFlags_Hideable | ImGuiTableFlags_HighlightHoveredColumn |
					ImGuiTableFlags_Reorderable | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
					ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit
				))
				{
					ImGui::TableSetupColumn("Checks");
					ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableHeadersRow();

					int _availableLocs = 0;
					for (const auto& songID : recvIDs) {
						auto loc1checked = std::find(CheckedLocations.begin(), CheckedLocations.end(), songID * 10) != CheckedLocations.end();
						auto loc2checked = std::find(CheckedLocations.begin(), CheckedLocations.end(), (songID * 10) + 1) != CheckedLocations.end();

						int available = (int)!loc1checked + (int)!loc2checked;

						if (autoremove && available == 0)
							continue;

						_availableLocs += available;

						ImGui::PushID(songID);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);

						std::string label = (available > 0) ? std::to_string(available) : " ";
						std::string label2 = (songID == APClient::victoryID / 10) ? "GOAL" : label;

						CenterText(label2);
						ImGui::Text("%s", label2.c_str());

						ImGui::TableSetColumnIndex(1);
						std::string name = item_ap_id_to_name[songID * 10];

						if (name.length() > 0)
						{
							bool isHinted = std::find(HintedIDs.begin(), HintedIDs.end(), songID) != HintedIDs.end();

							if (isHinted)
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));

							ImGui::Text("%s", name.c_str());

							if (isHinted)
								ImGui::PopStyleColor();
						}
						else {
							ImGui::Text("ID#%d (not in/load a datapackage)", songID);
						}

						if (APClient::devMode)
						{
							if (ImGui::BeginPopupContextItem("##xx"))
							{
								if (ImGui::MenuItem("Cheat##xx"))
									APClient::LocationSend(songID);

								ImGui::EndPopup();
							}
						}

						ImGui::PopID();
					}

					if (_availableLocs == 0)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						CenterText("?");
						ImGui::Text("?");
						ImGui::TableSetColumnIndex(1);
						ImGui::Text("Waiting for songs...");
					}

					availableLocs = _availableLocs;

					ImGui::EndTable();
				}

				ImGui::EndChild();
			}

			ImGui::EndTabItem();
		}
	}
}
