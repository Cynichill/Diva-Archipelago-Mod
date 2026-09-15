#include "APClient.h"
#include "APHints.h"
#include "APIDHandler.h"
#include "APReload.h"

namespace APIDHandler
{
	// Configurables
	bool freeplay = false;
	bool hide_checked = true; // Settings option
	bool slowRelease = false;
	int slowReleaseInterval = 180;

	// Internal
	bool reloading = false;
	std::chrono::system_clock::time_point slowReleaseNext;

	auto &CheckedLocations = APClient::CheckedLocations;
	auto &seedIDs = APClient::seedIDs;
	auto &recvIDs = APClient::recvIDs;
	auto &missingIDs = APClient::missingIDs;
	auto &item_ap_id_to_name = APClient::item_ap_id_to_name;
	int availableLocs = 0; // Calculated on tracker update

	bool queuedTrackerSort = false; // If true, the next time the table is visible run a sort.
	std::vector<TrackerItem> TrackerItems; // Updates when refreshTracker(). Sorted when visible and queuedTrackerSort or the sort spec is dirty.
	std::string trackerLine; // Holds the formatted Tracker line (Songs: #/# ...) from refreshTracker()

	auto &HintedIDs = APHints::HintedIDs;

	void config(const toml::table& settings)
	{

		toml::table section;
		if (settings.contains("tracker") && settings["tracker"].is_table())
			section = *settings["tracker"].as_table();

		hide_checked = section["hide_checked"].value_or(true);
		APLogger::print("hide_checked: %d\n", hide_checked);
	}

	void save(toml::table& settings)
	{
		toml::table config;
		config.insert("hide_checked", hide_checked);

		settings.insert("tracker", config);
	}

	bool check(const std::string& line)
	{
		if (missingIDs.size() == 0 || line.find("pv_") != 0 /*|| AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated*/)
			return true;

		size_t diff_pos = line.find(".difficulty.");
		size_t len_pos = line.rfind(".length");

		if (diff_pos == std::string::npos || len_pos == std::string::npos)
			return true;

		// Naively restrict to pv_#.difficulty.easy.length (difficulty and length already confirmed in the string)
		if (3 != std::count(line.begin(), line.end(), '.'))
			return true;

		size_t start = line.find_first_of("_");
		int64_t pvID = std::stoll(line.substr(start + 1, line.find_first_of(".") - start - 1));

		// Always enabled to prevent softlocks or crashing.
		if (144 == pvID || 700 == pvID)
			return true;

		auto &IDlist = freeplay ? missingIDs : recvIDs;
		auto contains = std::ranges::find(IDlist, pvID) != IDlist.end();

		if (!freeplay && contains && hide_checked)
		{
			bool loc1 = false;
			bool loc2 = false;

			for (const auto& locID : CheckedLocations) {
				if (locID == pvID * AP_ID_FACTOR) loc1 = true;
				if (locID == (pvID * AP_ID_FACTOR) + 1) loc2 = true;
				if (loc1 && loc2) break;
			}

			if (loc1 && loc2)
				return false;
		}

		return freeplay ? !contains : contains;
	}

	void reset()
	{
		//APLogger::print("IDHandler reset\n");
		freeplay = false;
		slowRelease = false;
		refreshTracker();
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

	void queueTrackerSort()
	{
		queuedTrackerSort = true;
	}

	void sortTrackerItems(const ImGuiTableSortSpecs* sort_specs)
	{
		// Always sort. Not too bad due to running from a CB.
		if (TrackerItems.size() > 1 /* && sort_specs->SpecsDirty */) {
			std::sort(
				TrackerItems.begin(), TrackerItems.end(),
				[sort_specs](TrackerItem a, TrackerItem b)
				{
					if (sort_specs->Specs->ColumnIndex == 0)
						return a.checksAvailable > b.checksAvailable;
					else if (sort_specs->Specs->ColumnIndex == 1)
						return a.receivedIndex > b.receivedIndex;
					else if (sort_specs->Specs->ColumnIndex == 2)
						return a.songID > b.songID;

					return a.name > b.name;
				}
			);
			if (sort_specs->Specs->SortDirection == ImGuiSortDirection_Descending)
				std::reverse(TrackerItems.begin(), TrackerItems.end());
		}
	}

	void refreshTracker()
	{
		TrackerItems.clear();
		int index = 0; // TODO: Track from Client instead? There will be gaps, but closer to web tracker.
		availableLocs = 0;
		for (const auto& songID : recvIDs) {
			index += 1;

			auto loc1checked = std::ranges::find(CheckedLocations, songID * AP_ID_FACTOR) == CheckedLocations.end();
			auto loc2checked = std::ranges::find(CheckedLocations, songID * AP_ID_FACTOR + 1) == CheckedLocations.end();
			int available = (int)loc1checked + (int)loc2checked;

			if (hide_checked && available == 0)
				continue;

			if (songID != APClient::victoryID / AP_ID_FACTOR)
				availableLocs += available;

			TrackerItem it;

			it.checksAvailable = available;
			it.name = item_ap_id_to_name[songID * AP_ID_FACTOR];
			it.songID = songID;
			it.receivedIndex = index;

			TrackerItems.push_back(it);
		}

		int64_t totalLocs = seedIDs.empty() ? 0 : (seedIDs.size() - 1) * 2;
		int64_t foundLocs = min(static_cast<int64_t>(CheckedLocations.size()), totalLocs);
		APClient::locHave = static_cast<int>(foundLocs);

		std::ostringstream trackerStream;
		trackerStream << "Songs: " << recvIDs.size() << "/" << seedIDs.size() << " | ";
		trackerStream << "Locs: " << foundLocs << "/";
		if (APClient::locNeed > 0)
			trackerStream << APClient::locNeed << "/";
		trackerStream << totalLocs << " | ";
		trackerStream << "Logic: " << availableLocs;

		if (APClient::leekNeed > 0)
			trackerStream << " | Leeks: " << APClient::leekHave << "/" << APClient::leekNeed;

		trackerLine = trackerStream.str();

		// Dump for i.e. stream overlay
		//std::ofstream tracker("stats.txt");
		//tracker << trackerLine;
	}

	void slowReleaseTouch()
	{
		slowReleaseNext = std::chrono::system_clock::now() + std::chrono::seconds(slowReleaseInterval);
	}

	void slowReleaseRun()
	{
		if (!slowRelease || AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated ||
			availableLocs == 0 || std::chrono::system_clock::now() < slowReleaseNext)
			return;

		for (const auto& item : TrackerItems) {
			if (item.checksAvailable == 0 /*|| item.songID == APClient::victoryID / AP_ID_FACTOR*/)
				continue; // If 'Hide checked' is false

			APLogger::print("Slow released: %s\n", item.name.c_str());
			APClient::LocationSend(item.songID);

			break;
		}

		slowReleaseTouch();
	}

	void ImGuiTab()
	{
		if (ImGui::CalcTextSize(trackerLine.c_str()).x < ImGui::GetContentRegionAvail().x)
			CenterText(trackerLine);
		//ImGui::PushTextWrapPos(ImGui::GetCursorPosX());
		ImGui::TextWrapped(trackerLine.c_str());
		//ImGui::PopTextWrapPos();

		ImGui::Separator();

		if (ImGui::BeginTable("tableTrackerOptions", 2, ImGuiTableFlags_SizingStretchSame))
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			if (ImGui::Checkbox("Freeplay", &freeplay))
				if (!ImGui::GetIO().KeyShift) APReload::run();
			HelpMarker("The entire song list will be available except for songs that have not been received yet.\nDeath Link and Traps still apply.\nShift+Click to not reload.");

			ImGui::TableSetColumnIndex(1);

			if (ImGui::Checkbox("Hide checked", &hide_checked)) {
				queuedTrackerSort = true;
				if (!ImGui::GetIO().KeyShift) APReload::run();
			}
			HelpMarker("When not in Freeplay, the song list will only show songs that have checks.\nShift+Click to not reload.");

			ImGui::EndTable();
		}

		if (APClient::devMode) {
			if (ImGui::Checkbox("Slow release every", &slowRelease))
				slowReleaseTouch();
			HelpMarker("Clears an unchecked song at the given interval.\nSends in listed, sorted order.\nDoes not prioritize hints.");
			ImGui::SameLine();
			ImGui::PushItemWidth(min(ImGui::GetContentRegionAvail().x * 0.25f, 80.0f));
			if (ImGui::SliderInt("seconds", &slowReleaseInterval, 60, 300, "%d"))
				slowReleaseInterval = max(1, slowReleaseInterval);
			ImGui::PopItemWidth();
			if (slowRelease) {
				ImGui::SameLine();
				ImGui::BeginDisabled();
				ImGui::Text("%.1fs", max(0.0f, std::chrono::duration<float>(slowReleaseNext - std::chrono::system_clock::now()).count()));
				ImGui::EndDisabled();
			}
		}

		if (ImGui::BeginTable("tableTracker", 4,
			ImGuiTableFlags_Sortable |
			ImGuiTableFlags_BordersInner | ImGuiTableFlags_Hideable | ImGuiTableFlags_HighlightHoveredColumn |
			ImGuiTableFlags_Reorderable | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit
		))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Checks");
			ImGui::TableSetupColumn("Received", ImGuiTableColumnFlags_DefaultHide);
			ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultHide);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort);
			ImGui::TableHeadersRow();

			ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
			if (sort_specs && (queuedTrackerSort || sort_specs->SpecsDirty)) {
				refreshTracker();
				sortTrackerItems(sort_specs);
			}

			for (const auto& item : TrackerItems) {
				ImGui::PushID(static_cast<int>(item.songID));

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				std::string label = (item.checksAvailable > 0) ? std::to_string(item.checksAvailable) : " ";
				label = (item.songID == APClient::victoryID / AP_ID_FACTOR) ? "GOAL" : label;

				CenterText(label);
				ImGui::Text("%s", label.c_str());

				ImGui::TableNextColumn();

				CenterText(std::to_string(item.receivedIndex));
				ImGui::Text("%i", item.receivedIndex);

				ImGui::TableNextColumn();

				CenterText(std::to_string(item.songID));
				ImGui::Text("%i", item.songID);

				ImGui::TableNextColumn();
				std::string name = item_ap_id_to_name[item.songID * AP_ID_FACTOR];
				if (name.empty())
					name = "ID " + std::to_string(item.songID) + " (not in datapackage)";

				if (*(bool*)PvPlayData && item.songID == static_cast<int64_t>(*(int*)(PvPlayData + 0x10)))
					name = "NP: " + name;

				bool isHinted = std::ranges::find(HintedIDs, item.songID) != HintedIDs.end();

				if (isHinted)
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));

				ImGui::Text("%s", name.c_str());

				if (isHinted)
					ImGui::PopStyleColor();

				ImGui::TableSetColumnIndex(0);

				ImGui::Selectable("##xx", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);

				if (ImGui::BeginPopupContextItem("##xx"))
				{
					if (APClient::devMode) {
						if (ImGui::MenuItem("Cheat##xx"))
							APClient::LocationSend(item.songID);
						ImGui::Separator();
					}

					if (ImGui::MenuItem("Copy song name##xx"))
						ImGui::SetClipboardText(item.name.c_str());

					if (ImGui::MenuItem("Copy song ID##xx"))
						ImGui::SetClipboardText(std::to_string(item.songID).c_str());

					ImGui::EndPopup();
				}

				ImGui::PopID();
			}

			if (TrackerItems.size() == 0)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				CenterText("BK");
				ImGui::Text("BK");
				ImGui::TableSetColumnIndex(3);
				ImGui::Text("Waiting for songs...");
			}

			ImGui::EndTable();
		}
	}
}
