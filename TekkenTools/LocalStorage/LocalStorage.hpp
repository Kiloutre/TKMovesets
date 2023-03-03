#pragma once

#include <sys/stat.h>
#include <set>
#include <vector>

#include "ThreadedClass.hpp"

// Moveset info, most of which is read straight from the file. Use for display.
struct movesetInfo
{
	std::string filename;
	std::string name;
	std::string origin;
	std::string target_character;
	std::string version_string;
	std::string date;
	float size;
	time_t modificationDate;
};

class LocalStorage : public ThreadedClass
{
private:
	// Contains the list of movesets found in the extraction directory
	std::vector<movesetInfo*> localMovesets;
	// A set containing the list of moveset files in the configured extraction dir. Used to determine when to fetch new movesets info
	std::set<std::string> m_extractedMovesetFilenames;
	// .extractedMovesets garbage, because it can be accessed in another thread while we remove items for it.
	std::vector<movesetInfo*> m_garbage;

	// Function ran in the parallel thread, used to list files regularly
	void Update() override;
public:
	// Contains the list of movesets found in the extraction directory
	std::vector<movesetInfo*> extractedMovesets;

	// Stops the thread started above
	void StopThreadAndCleanup() override;
	// Reads movesets from their configured extraction dir. Accessible under .extractedMovesets
	void ReloadMovesetList();
	// Frees the content of .garbage. Must be called by the display thread after being finished with .extractedMovesets
	void CleanupUnusedMovesetInfos();
	// Delete a moveset's file entirely
	void DeleteMoveset(const char* filename);
};