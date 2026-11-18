#pragma once

#include <set>
#include <map>
#include <chrono>
#include <memory>
#include <filesystem>
#include "constants.h"
#include <mutex>
#include <vector>

#include "ThreadedClass.hpp"
#include "Helpers.hpp"

// Moveset info, most of which is read straight from the file. Use for display.
struct movesetInfo
{
	int color; // Color is defined by moveset flags
	std::wstring filename;
	std::string name;
	std::wstring wname;
	FasterStringComp origin;
	std::string target_character;
	std::string original_character;
	std::string version_string;

	uint64_t date;
	std::string date_str;

	std::string extraction_date_str;

	uint64_t size;
	std::string sizeStr;

	std::string hash;

	uint64_t extractionDate;
	time_t modificationDate;
	uint16_t gameId;
	uint16_t minorVersion;
	bool modified; // Was modified with the editor
	bool editable;
	bool onlineImportable; // Moveset over a certain size can't be used online

	bool is_valid;
	// Retail T8 notes section (RetailT8Snapshot::NotesTag), empty when the file has nothing to say.
	std::string notes;
};

class LocalStorage : public ThreadedClass
{
private:
	std::filesystem::path m_directory;
    bool m_useConfiguredFolders=false;
	// Contains the list of movesets found in the extraction directory
	std::vector<std::shared_ptr<movesetInfo>> localMovesets;
	std::vector<std::shared_ptr<movesetInfo>> m_publishedMovesets;
	// A set containing the list of moveset files in the configured extraction dir. Used to determine when to fetch new movesets info
	std::set<std::wstring> m_extractedMovesetFilenames;
	// Files that read as invalid, maybe because they were still being written: how many times they were read, and when to read them again
	struct InvalidRecheck { unsigned reads; std::chrono::steady_clock::time_point next; };
	std::map<std::wstring, InvalidRecheck> m_invalidRechecks;
	// If true, will clear the moveset list and refresh it in the next run
	std::atomic_bool m_refreshMovesets = false;
	std::mutex m_listMutex;

	// Function ran in the parallel thread, used to list files regularly
	void Update() override;
public:
	// Contains the list of movesets found in the extraction directory
	std::vector<movesetInfo*> extractedMovesets;

    LocalStorage() : m_directory(MOVESET_DIRECTORY), m_useConfiguredFolders(true) {}
	explicit LocalStorage(std::filesystem::path directory) : m_directory(std::move(directory)) {}
	~LocalStorage();

	// Stops the thread started above
	void StopThreadAndCleanup() override;
	// Reads movesets from their configured extraction dir. Accessible under .extractedMovesets
	// waitForWrites: leave files written in the last few milliseconds for a later call, which the periodic scan makes
	void ReloadMovesetList(bool waitForWrites = false);
	// Frees the content of .garbage. Must be called by the display thread after being finished with .extractedMovesets
	void CleanupUnusedMovesetInfos();
	// Delete a moveset's file entirely, returns false if deletion failed
	bool DeleteMoveset(const wchar_t* filename);
	// Force movesets to refresj
	void RefreshMovesets();
	// Reads one moveset file's header, wherever it is. Null if the file is not a valid moveset.
	static std::unique_ptr<movesetInfo> ReadMovesetInfo(const std::wstring& filename);
};
