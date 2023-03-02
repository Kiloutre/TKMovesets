#pragma once

#include <sys/stat.h>
#include <set>
#include <vector>

#include "ThreadedClass.hpp"

// Data contained at the head of every extracted movesets
struct MovesetHeader
{
	// ID of the extracted character, used internally to make some moves that require it work
	int32_t characterId;
	// Version string of the extracted moveset
	char version_string[28];
	// Flags used for storing useful data. Currently unused. Todo : see what we can do with this?
	int64_t flags;
	// Origin (Game name + author if you want)
	char origin[32];
	// Target character to play on
	char target_character[32];
	// Date of last modification
	char date[24];

	// Offsets of the various blocks contained within the moveset, in the same order they will appear in
	int64_t movesetInfoBlockOffset;
	int64_t listsBlockOffset;
	int64_t nameBlockOffset;
	int64_t movesetBlockOffset;
	int64_t animationBlockOffset; // Custom block
	int64_t motaBlockOffset; // Custom block
};

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