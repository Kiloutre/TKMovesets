#include <chrono>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <windows.h>

#include "LocalStorage.hpp"
#include "Helpers.hpp"

#include "constants.h"

// -- Static helpers --

// Reads a movest in order to fetch its header informations such as character name, version, etc. Can return null if errors are encountered.
static movesetInfo* fetchMovesetInformations(std::string filename)
{
	std::ifstream file;
	file.open(filename, std::ios::binary);

	if (!file.fail()) {

		MovesetHeader header{ 0 };
		file.read((char*)&header, sizeof(MovesetHeader));

		size_t readBytes = file.gcount();
		file.seekg(0, std::ios::end);
		size_t totalSize = file.tellg();
		file.close();

		if (readBytes != sizeof(MovesetHeader) ||
			Helpers::isHeaderStringMalformated(header.origin, sizeof(header.origin)) ||
			Helpers::isHeaderStringMalformated(header.origin, sizeof(header.target_character)) ||
			Helpers::isHeaderStringMalformated(header.origin, sizeof(header.version_string)) ||
			Helpers::isHeaderStringMalformated(header.origin, sizeof(header.date))) {
			// File malformated
		}
		else {
			struct stat buffer;
			// stat() the file to read modification time
			stat(filename.c_str(), &buffer);

			return new movesetInfo{
				.filename = filename,
				.name = Helpers::getMovesetNameFromFilename(filename),
				.origin = std::string(header.origin),
				.target_character = std::string(header.target_character),
				.date = std::string(header.date),
				.size = (float)totalSize / 1000 / 1000,
				.modificationDate = buffer.st_mtime
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
	CreateDirectory(MOVESET_DIRECTORY, NULL);

	for (const auto& entry : std::filesystem::directory_iterator(MOVESET_DIRECTORY))
	{
		// todo: see how this works with utf8 chars
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
				   .filename = filename,
				   .name = Helpers::getMovesetNameFromFilename(filename),
				   .origin = std::string("INVALID"),
				   .target_character = std::string(""),
				   .date = std::string(""),
				   .size = 0,
				   .modificationDate = 0
				};
			}

			extractedMovesets.push_back(moveset);
		}
	}

	// Delete moveset entries that don't exist anymore (deleted files)
	for (size_t i = 0; i < extractedMovesets.size();) {
		movesetInfo* moveset = extractedMovesets[i];
		struct stat buffer;

		if ((stat(moveset->filename.c_str(), &buffer) != 0) || buffer.st_mtime != moveset->modificationDate) {
			// File does not exist anymore, de-allocate the info we stored about it
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