#include "Importer.hpp"
#include <memory>
#include "Compression.hpp"
#include "GameIDs.hpp"
#include "Helpers.hpp"

#include "MovesetStructs.h"

// -- Static helpers -- //

// -- Public -- //

ImportationErrcode_ Importer::Import(const wchar_t* filename, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	progress = 0;
	// Read file data

	DEBUG_LOG("Loading file '%S'\n", filename);
	progress = 5;

	// Variables that will store the moveset size & the moveset itself in our own memory
	uint64_t s_moveset;
	Byte* moveset;
;
	// Allocate a copy of the moveset locally. This is NOT in the game's memory
    try {
        moveset = Helpers::ReadMovesetFile(filename, s_moveset);
    } catch(const MovesetFile_AllocationError&)
    {
		return ImportationErrcode_AllocationErr;
    } catch(const std::exception&)
    {
		return ImportationErrcode_FileReadErr;
    }
    
	progress = 10;

	ImportationErrcode_ errcode = ImportChecked(moveset, s_moveset, playerAddress, settings, progress);

	free(moveset);
	return errcode;
}

ImportationErrcode_ Importer::Import(const Byte* orig_moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	if (!orig_moveset || s_moveset < sizeof(TKMovesetHeader) || !((TKMovesetHeader*)orig_moveset)->ValidateHeader(s_moveset)) {
		return ImportationErrcode_MovesetDataError;
	}

	Byte* moveset = (Byte*)malloc(s_moveset);
	progress = 5;
	if (moveset == nullptr) {
		return ImportationErrcode_AllocationErr;
	}

	memcpy(moveset, orig_moveset, s_moveset);
	progress = 10;

	ImportationErrcode_ errcode = ImportChecked(moveset, s_moveset, playerAddress, settings, progress);

	free(moveset);
	return errcode;
}

ImportationErrcode_ Importer::_Import(Byte* moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	progress = 15;

	// Header of the moveset that will contain our own information about it
	const TKMovesetHeader* header = (TKMovesetHeader*)moveset;

	bool isCompressed = header->isCompressed();
    std::unique_ptr<Byte[]> expanded;

	if (settings & ImportSettings_ImportOriginalData) {
		const gameAddr gameOriginalMoveset = m_process.allocateMem(s_moveset);
		if (gameOriginalMoveset == 0) {
			return ImportationErrcode_GameAllocationErr;
		}
		if (!m_process.writeBytes(gameOriginalMoveset, moveset, s_moveset)) {
			m_process.freeMem(gameOriginalMoveset);
			return ImportationErrcode_GameAllocationErr;
		}
		lastLoaded.originalDataAddress = gameOriginalMoveset;
		lastLoaded.originalDataSize = s_moveset;
	}
	else {
		lastLoaded.originalDataAddress = 0;
		lastLoaded.originalDataSize = 0;
	}

	if (isCompressed) {
		uint64_t compressed_data_size = s_moveset - header->moveset_data_start;

		moveset = CompressionUtils::RAW::Moveset::Decompress(moveset, compressed_data_size, s_moveset);

        expanded.reset(moveset);
		if (moveset == nullptr) {
			if (lastLoaded.originalDataAddress) m_process.freeMem(lastLoaded.originalDataAddress);
			lastLoaded.originalDataAddress = lastLoaded.originalDataSize = 0;
			return ImportationErrcode_DecompressionError;
		}
	}
	else {
		// Not compressed
		// Go past the header now that we have a ptr to the header. This will be what is sent to the game.
		s_moveset -= header->moveset_data_start;
		moveset += header->moveset_data_start;
	}
	progress = 20;

	ImportationErrcode_ errCode;
	errCode = ImportMovesetData(header, moveset, s_moveset, playerAddress, settings, progress);


	if (errCode != ImportationErrcode_Successful && lastLoaded.originalDataAddress) {
		m_process.freeMem(lastLoaded.originalDataAddress);
		lastLoaded.originalDataAddress = lastLoaded.originalDataSize = 0;
	}
	return errCode;
}
ImportationErrcode_ Importer::ImportChecked(Byte* bytes, uint64_t size, gameAddr player, ImportSettings settings, uint8_t& progress)
{
    // T7 (including converted formats) and retail T8 stage writes. Retail T8
    // also pauses the process during checked upload/publication and rollback.
    if (m_gameInfo.gameId != GameId_T7 && !(m_gameInfo.gameId == GameId_T8 && m_gameInfo.minorVersion == GameVersions::T8::RETAIL))
        return _Import(bytes, size, player, settings, progress);
    const auto previous = lastLoaded;
    try {
        GameProcess::WriteBatch transaction(m_process);
        auto error = _Import(bytes, size, player, settings, progress);
        if (error != ImportationErrcode_Successful) { lastLoaded = previous; return error; }
        std::unique_ptr<GameProcess::PauseGuard> pause;
        if (m_gameInfo.gameId == GameId_T8)
            pause = std::make_unique<GameProcess::PauseGuard>(m_process);
        const auto result = transaction.Commit();
        if (result.ok) return ImportationErrcode_Successful;
        lastLoaded = previous; progress = 0;
        return result.rollbackComplete ? ImportationErrcode_GameWriteError : ImportationErrcode_GameRollbackError;
    } catch (const std::bad_alloc&) {
        lastLoaded = previous; progress = 0; return ImportationErrcode_AllocationErr;
    } catch (const std::exception&) {
        lastLoaded = previous; progress = 0; return ImportationErrcode_GameWriteError;
    }
}
