#include <chrono>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <windows.h>

#include "LocalStorage.hpp"
#include "Helpers.hpp"
#include "Games.hpp"

#include "MovesetStructs.h"
#include "constants.h"

// -- Static helpers --

static int getMovesetColor(MovesetFlags flags)
{
	if (flags & MovesetFlags_MOVESET_MODIFIED) {
		return MOVESET_LIST_MODIFIED;
	}
	return 0;
}

// Reads a movest in order to fetch its header informations such as character name, version, etc. Can return null if errors are encountered.
static movesetInfo* fetchMovesetInformations(const std::string& filename)
{
	std::ifstream file;
	file.open(filename, std::ios::binary);

	if (!file.fail())
	{
		TKMovesetHeader header{ 0 };
		file.read((char*)&header, sizeof(TKMovesetHeader));

		size_t readBytes = file.gcount();
		file.seekg(0, std::ios::end);
		size_t totalSize = file.tellg();
		file.close();

		TKMovesetHeader_infos* movesetInfos = (TKMovesetHeader_infos*)&header;

		if (readBytes != sizeof(TKMovesetHeader) ||
			Helpers::isHeaderStringMalformated(movesetInfos->version_string, sizeof(movesetInfos->version_string)) ||
			Helpers::isHeaderStringMalformated(movesetInfos->origin, sizeof(movesetInfos->origin)) ||
			Helpers::isHeaderStringMalformated(movesetInfos->target_character, sizeof(movesetInfos->target_character))) {
			// File malformated
			return nullptr;
		}
		else {
			struct stat buffer;
			// stat() the file to read modification time
			stat(filename.c_str(), &buffer);

			return new movesetInfo{
				.color = getMovesetColor(movesetInfos->flags),
				.filename = filename,
				.name = Helpers::getMovesetNameFromFilename(filename),
				.origin = std::string(movesetInfos->origin),
				.target_character = std::string(movesetInfos->target_character),
				.version_string = std::string(movesetInfos->version_string),
				.date = movesetInfos->date,
				.date_str = Helpers::formatDateTime(movesetInfos->date),
				.size = (float)totalSize / 1000 / 1000,
				.modificationDate = buffer.st_mtime,
				.gameId = movesetInfos->gameId,
				.editable = Games::IsGameEditable(movesetInfos->gameId)
			};
		}

	}

	return nullptr;
}

// -- Private methods -- //

void LocalStorage::Update()
{
	// Executed in its own thread
	while (m_threadStarted)
	{
		// Update the list of moveset files. This is better than doing it in the display thread
		ReloadMovesetList();

		std::this_thread::sleep_for(std::chrono::milliseconds(GAME_INTERACTION_THREAD_SLEEP_MS));
	}
}

// -- Public methods -- //

void LocalStorage::StopThreadAndCleanup()
{
	// Order thread to stop
	if (m_threadStarted)
	{
		m_threadStarted = false;
		m_t.join();
	}

	// Free allocated ressources
	CleanupUnusedMovesetInfos();
	for (movesetInfo* movesetInfo : extractedMovesets) {
		delete movesetInfo;
	}
}

void LocalStorage::ReloadMovesetList()
{
	// Create extraction directory if it doesn't exist
	CreateDirectory(MOVESET_DIRECTORY, NULL);

	for (const auto& entry : std::filesystem::directory_iterator(MOVESET_DIRECTORY))
	{
		// Todo: Unicode .name support
		std::string filename = entry.path().string();

		if (!Helpers::endsWith(filename, MOVESET_FILENAME_EXTENSION)) {
			// Skip files that we do not recognize
			continue;
		}

		// If new file is detected, fetch its infos
		if (!m_extractedMovesetFilenames.contains(filename))
		{
			m_extractedMovesetFilenames.insert(filename);
			movesetInfo* moveset = fetchMovesetInformations(filename);

			if (moveset == nullptr) {
				moveset = new movesetInfo{
					.color = MOVESET_INVALID,
					.filename = filename,
					.name = Helpers::getMovesetNameFromFilename(filename),
					.origin = std::string("INVALID"),
					.target_character = std::string(""),
					.date = 0,
					.size = 0,
					.modificationDate = 0,
					.editable = false
				};
			}

			extractedMovesets.push_back(moveset);
		}
	}

	// Delete moveset entries that don't exist anymore (deleted files)
	for (size_t i = 0; i < extractedMovesets.size();) {
		movesetInfo* moveset = extractedMovesets[i];
		struct stat buffer;

		if ((stat(moveset->filename.c_str(), &buffer) != 0) || (moveset->modificationDate != 0 && buffer.st_mtime != moveset->modificationDate)) {
			// File does not exist anymore or was recently modified, de-allocate the info we stored about it
			// (We remove from the set FIRST because erasing from the vector calls the std::string destuctor)
			m_extractedMovesetFilenames.erase(m_extractedMovesetFilenames.find(moveset->filename));
			extractedMovesets.erase(extractedMovesets.begin() + i, extractedMovesets.begin() + i + 1);
			m_garbage.push_back(moveset);
		}
		else {
			++i;
		}
	}
}

void LocalStorage::CleanupUnusedMovesetInfos()
{
	while (m_garbage.size() > 0)
	{
		movesetInfo* moveset = m_garbage[0];
		m_garbage.erase(m_garbage.begin());
		delete moveset;
	}
}

void LocalStorage::DeleteMoveset(const char* filename)
{
	remove(filename);
}

LocalStorage::~LocalStorage()
{
	CleanupUnusedMovesetInfos();
}