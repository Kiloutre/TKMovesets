#include "Extractor_t8/RetailSnapshot.hpp"
#include "GameIDs.hpp"
#include <filesystem>
#include <fstream>
#include <sys/stat.h>

#include "LocalStorage.hpp"
#include "Localization.hpp"
#include "Helpers.hpp"
#include "Games.hpp"
#include "GameStaticData.hpp"
#include "MovesetFolders.hpp"

#include "MovesetStructs.h"
#include "constants.h"

// -- Static helpers --

static std::string displayCharacterName(const TKMovesetHeader& header, const char* stored)
{
    if (header.gameId != GameId_T8 || header.minorVersion != GameVersions::T8::RETAIL) return stored;
    const auto generic = "Character " + std::to_string(header.characterId);
    if (*stored && stored != generic) return stored;
    static const auto names = GameStaticData::LoadOptional("t8-retail");
    return names.CharacterName(header.characterId);
}

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

		if (readBytes != sizeof(TKMovesetHeader) || !movesetInfos.ValidateHeader(totalSize) ||
			Helpers::isHeaderStringMalformated(movesetInfos.version_string, sizeof(movesetInfos.version_string)) ||
			Helpers::isHeaderStringMalformated(movesetInfos.origin, sizeof(movesetInfos.origin)) ||
			Helpers::isHeaderStringMalformated(movesetInfos.target_character, sizeof(movesetInfos.target_character))) {
			// File malformated
			return nullptr;
		}
		else {
			struct _stat64 buffer{};
			// _wstat64() the file to read modification time
			_wstat64(filename.c_str(), &buffer);

			// Build crc32 str string
			std::string crc32Str;
			{
				char buffer[9];
				sprintf_s(buffer, 9, "%08X", movesetInfos.crc32);
				crc32Str = buffer;
			}

			// Retail T8 files may carry a notes section in their extended header.
			std::string notes;
			if (movesetInfos.gameId == GameId_T8 && movesetInfos.minorVersion == GameVersions::T8::RETAIL &&
				movesetInfos.header_size > sizeof(TKMovesetHeader) && movesetInfos.header_size <= (65u << 20) && movesetInfos.header_size <= totalSize) {
				try {
					std::vector<Byte> prefix(movesetInfos.header_size);
					std::ifstream again(filename, std::ios::binary);
					if (again.read(reinterpret_cast<char*>(prefix.data()), prefix.size())) notes = RetailT8Snapshot::Notes(prefix);
				} catch (const std::exception&) { notes.clear(); }
			}

			return new movesetInfo{
				.color = getMovesetColor(movesetInfos.flags),
				.filename = filename,
				.name = Helpers::getMovesetNameFromFilename(filename),
				.wname = Helpers::getMovesetWNameFromFilename(filename),
				.origin = movesetInfos.origin,
				.target_character = displayCharacterName(movesetInfos, movesetInfos.target_character),
				.original_character = displayCharacterName(movesetInfos, movesetInfos.GetOrigCharacterName()),
				.version_string = std::string(movesetInfos.version_string),
				.date = movesetInfos.date,
				.date_str = Helpers::formatDateTime(movesetInfos.date),
				.extraction_date_str = Helpers::formatDateTime(movesetInfos.extraction_date),
				.size = totalSize,
				.sizeStr = std::format("{:.2f} {}", (float)totalSize / 1000 / 1000, "MB"),
				.hash = crc32Str,
				.extractionDate = movesetInfos.extraction_date,
				.modificationDate = buffer.st_mtime,
				.gameId = movesetInfos.gameId,
				.minorVersion = movesetInfos.minorVersion,
				.modified = (movesetInfos.flags & MovesetFlags_MovesetModified) != 0,
				.editable = Games::IsGameEditable(movesetInfos.gameId, movesetInfos.minorVersion),
				.onlineImportable = totalSize < ONLINE_MOVESET_MAX_SIZE_BYTES,
				.is_valid = true,
				.notes = std::move(notes)
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
		try {
			ReloadMovesetList(true);
		} catch (const std::filesystem::filesystem_error&) {
			// Files can disappear during directory iteration. Retry on the next scan.
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(GAME_INTERACTION_THREAD_SLEEP_MS));
	}
}

// -- Public methods -- //

void LocalStorage::StopThreadAndCleanup()
{
	ThreadedClass::StopThreadAndCleanup();
    std::lock_guard lock(m_listMutex);
    extractedMovesets.clear();
    m_publishedMovesets.clear();
    localMovesets.clear();
    m_extractedMovesetFilenames.clear();
    m_invalidRechecks.clear();
}

void LocalStorage::ReloadMovesetList(bool waitForWrites)
{
	std::lock_guard lock(m_listMutex);
	if (m_refreshMovesets.exchange(false))
	{
        localMovesets.clear();
        m_extractedMovesetFilenames.clear();
        m_invalidRechecks.clear();
	}

	const auto now = std::chrono::steady_clock::now();

	// Delete moveset entries that don't exist anymore (deleted files)
	for (size_t i = 0; i < localMovesets.size();) {
		auto moveset = localMovesets[i];
		struct _stat64 buffer{};

		bool statFailed = _wstat64(moveset->filename.c_str(), &buffer) != 0;
		// An invalid file may have been read before it was fully written: drop its entry so it is read again
		const auto recheck = m_invalidRechecks.find(moveset->filename);
		const bool recheckDue = !moveset->is_valid && recheck != m_invalidRechecks.end() && now >= recheck->second.next;

		if (statFailed || recheckDue ||
			(buffer.st_mtime != moveset->modificationDate || buffer.st_size != moveset->size)) {
			// A file that changed or disappeared starts over with a fresh number of rechecks
			if (!recheckDue) m_invalidRechecks.erase(moveset->filename);
			// File does not exist anymore or was recently modified, de-allocate the info we stored about it
			// (We remove from the set FIRST because erasing from the vector calls the std::string destuctor)
			m_extractedMovesetFilenames.erase(m_extractedMovesetFilenames.find(moveset->filename));
			localMovesets.erase(localMovesets.begin() + i);
		}
		else {
			++i;
		}
	}

    std::set<std::wstring> seen;
    const auto directories=m_useConfiguredFolders ? MovesetFolders::SearchDirectories() : std::vector<std::filesystem::path>{m_directory};
    for (const auto& directory:directories)
	{
        std::error_code directoryError;
        auto iterator=std::filesystem::directory_iterator(directory,std::filesystem::directory_options::skip_permission_denied,directoryError);
		for (; !directoryError && iterator!=std::filesystem::directory_iterator(); iterator.increment(directoryError))
		{
			const auto& entry=*iterator;
			std::filesystem::path file = entry.path();
			std::wstring filename = file.wstring();
		

			std::error_code ec;
			if (file.extension() != L"" MOVESET_FILENAME_EXTENSION || !entry.is_regular_file(ec) || entry.file_size(ec) == 0 || ec) {
				// Skip files that we do not recognize or that are not yet fully written
				continue;
			}
            auto canonical=std::filesystem::weakly_canonical(file,ec);
            if(ec)continue;
            filename=canonical.wstring();
            if(!seen.insert(filename).second)continue;

			// If new file is detected, fetch its infos
			if (!m_extractedMovesetFilenames.contains(filename))
			{
				// Give a file that was just written a moment to settle before reading it; the next scan picks it up
				if (waitForWrites) {
					const auto lastWrite = entry.last_write_time(ec);
					if (ec || std::filesystem::file_time_type::clock::now() - lastWrite < std::chrono::milliseconds(MOVESET_SETTLE_DELAY_MS)) continue;
				}

				m_extractedMovesetFilenames.insert(filename);
				movesetInfo* moveset = fetchMovesetInformations(filename);
				struct _stat64 fileStat{};
				_wstat64(filename.c_str(), &fileStat);

				if (moveset != nullptr) m_invalidRechecks.erase(filename);
				else {
					auto& recheck = m_invalidRechecks[filename];
					// After the last read the file stays invalid until it changes
					if (++recheck.reads <= MOVESET_INVALID_RECHECKS) recheck.next = now + std::chrono::milliseconds(MOVESET_INVALID_RECHECK_DELAY_MS);
					else recheck.next = (std::chrono::steady_clock::time_point::max)();
				}

				if (moveset == nullptr) {
					moveset = new movesetInfo{
						.color = MOVESET_INVALID,
						.filename = filename,
						.name = Helpers::getMovesetNameFromFilename(filename),
						.origin = std::string("INVALID"),
						.target_character = "",
						.original_character = "",
						.date = 0,
						.size = static_cast<uint64_t>(fileStat.st_size),
						.modificationDate = fileStat.st_mtime,
						.modified = false,
						.editable = false,
						.is_valid = false
					};
				}

				localMovesets.emplace_back(moveset);
			}
		}
	}
    std::erase_if(localMovesets,[&](const auto& moveset) {
        if(seen.contains(moveset->filename))return false;
        m_extractedMovesetFilenames.erase(moveset->filename);m_invalidRechecks.erase(moveset->filename);return true;
    });
}

void LocalStorage::CleanupUnusedMovesetInfos()
{
	std::unique_lock lock(m_listMutex, std::try_to_lock);
	if (!lock) return;
    m_publishedMovesets = localMovesets;
    extractedMovesets.clear();
    for (const auto& moveset : m_publishedMovesets) extractedMovesets.push_back(moveset.get());
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
	StopThreadAndCleanup();
}

void LocalStorage::RefreshMovesets()
{
	m_refreshMovesets = true;
}

std::unique_ptr<movesetInfo> LocalStorage::ReadMovesetInfo(const std::wstring& filename)
{
	return std::unique_ptr<movesetInfo>(fetchMovesetInformations(filename));
}
