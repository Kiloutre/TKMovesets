#include <filesystem>
#include <fstream>
#include <sys/stat.h>

#include "LocalStorage.hpp"
#include "Localization.hpp"
#include "Helpers.hpp"
#include "Games.hpp"

#include "MovesetStructs.h"
#include "constants.h"

// -- Static helpers --

static int getMovesetColor(MovesetFlags flags)
{
	if (flags & MovesetFlags_MovesetModified) {
		return MOVESET_LIST_MODIFIED;
	}
	return 0;
}

// Reads a movest in order to fetch its header informations such as character name, version, etc. Can return null if errors are encountered.
static movesetInfo* fetchMovesetInformations(const std::wstring& filename)
{
	std::ifstream file;
	file.open(filename, std::ios::binary);

	if (!file.fail())
	{
		TKMovesetHeader movesetInfos;
		file.read((char*)&movesetInfos, sizeof(TKMovesetHeader));

		size_t readBytes = file.gcount();
		file.seekg(0, std::ios::end);
		size_t totalSize = file.tellg();
		file.close();

		if (readBytes != sizeof(TKMovesetHeader) || !movesetInfos.ValidateHeader() ||
			Helpers::isHeaderStringMalformated(movesetInfos.version_string, sizeof(movesetInfos.version_string)) ||
			Helpers::isHeaderStringMalformated(movesetInfos.origin, sizeof(movesetInfos.origin)) ||
			Helpers::isHeaderStringMalformated(movesetInfos.target_character, sizeof(movesetInfos.target_character))) {
			// File malformated
			return nullptr;
		}
		else {
			struct _stat buffer;
			// _wstat() the file to read modification time
			_wstat(filename.c_str(), &buffer);

			// Build crc32 str string
			std::string crc32Str;
			{
				char buffer[9];
				sprintf_s(buffer, 9, "%08X", movesetInfos.crc32);
				crc32Str = buffer;
			}

			return new movesetInfo{
				.color = getMovesetColor(movesetInfos.flags),
				.filename = filename,
				.name = Helpers::getMovesetNameFromFilename(filename),
				.wname = Helpers::getMovesetWNameFromFilename(filename),
				.origin = movesetInfos.origin,
				.target_character = movesetInfos.target_character,
				.original_character = movesetInfos.GetOrigCharacterName(),
				.version_string = std::string(movesetInfos.version_string),
				.date = movesetInfos.date,
				.date_str = Helpers::formatDateTime(movesetInfos.date),
				.size = totalSize,
				.sizeStr = std::format("{:.2f} {}", (float)totalSize / 1000 / 1000, _("moveset.size_mb")),
				.hash = crc32Str,
				.extractionDate = movesetInfos.extraction_date,
				.modificationDate = buffer.st_mtime,
				.gameId = movesetInfos.gameId,
				.minorVersion = movesetInfos.minorVersion,
				.modified = (movesetInfos.flags & MovesetFlags_MovesetModified) != 0,
				.editable = Games::IsGameEditable(movesetInfos.gameId, movesetInfos.minorVersion),
				.onlineImportable = totalSize < ONLINE_MOVESET_MAX_SIZE_BYTES,
				.is_valid = true
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
	if (m_refreshMovesets)
	{
		for (unsigned int i = 0; i < extractedMovesets.size();)
		{
			auto m = extractedMovesets[i];
			m_garbage.push_back(m);
			m_extractedMovesetFilenames.erase(m_extractedMovesetFilenames.find(m->filename));
			extractedMovesets.erase(extractedMovesets.begin());
		}
		m_refreshMovesets = false;
	}

	if (Helpers::fileExists(L"" MOVESET_DIRECTORY))
	{
		for (const auto& entry : std::filesystem::directory_iterator(L"" MOVESET_DIRECTORY))
		{
			std::wstring filename = entry.path().wstring();
		

			if (!Helpers::endsWith<std::wstring>(filename, L"" MOVESET_FILENAME_EXTENSION) || entry.file_size() == 0) {
				// Skip files that we do not recognize or that are not yet fully written
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
						.target_character = "",
						.original_character = "",
						.date = 0,
						.size = 0,
						.modificationDate = 0,
						.modified = false,
						.editable = false,
						.is_valid = false
					};
				}

				extractedMovesets.push_back(moveset);
			}
		}
	}

	// Delete moveset entries that don't exist anymore (deleted files)
	for (size_t i = 0; i < extractedMovesets.size();) {
		movesetInfo* moveset = extractedMovesets[i];
		struct _stat buffer;

		bool statFailed = _wstat(moveset->filename.c_str(), &buffer) != 0;

		if (statFailed ||
			(moveset->modificationDate != 0 && (buffer.st_mtime != moveset->modificationDate || buffer.st_size != moveset->size))) {
			// File does not exist anymore or was recently modified, de-allocate the info we stored about it
			// (We remove from the set FIRST because erasing from the vector calls the std::string destuctor)
			m_extractedMovesetFilenames.erase(m_extractedMovesetFilenames.find(moveset->filename));
			extractedMovesets.erase(extractedMovesets.begin() + i);
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

bool LocalStorage::DeleteMoveset(const wchar_t* filename)
{
	try {
		std::filesystem::remove(filename);
	}
	catch (std::filesystem::filesystem_error const&) {
		DEBUG_ERR("DELETION OF MOVESET '%S' FAILED", filename);
		return false;
	}
	return true;
}

LocalStorage::~LocalStorage()
{
	CleanupUnusedMovesetInfos();
}

void LocalStorage::RefreshMovesets()
{
	if (m_refreshMovesets) return;
	DEBUG_LOG("RefreshMovesets start\n");

	m_refreshMovesets = true;
	while (!m_refreshMovesets)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		continue;
	}

	DEBUG_LOG("RefreshMovesets end\n");
}