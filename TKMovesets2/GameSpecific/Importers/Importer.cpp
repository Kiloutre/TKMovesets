#include "Importer.hpp"
#include "Compression.hpp"
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

	ImportationErrcode_ errcode = _Import(moveset, s_moveset, playerAddress, settings, progress);

	free(moveset);
	return errcode;
}

ImportationErrcode_ Importer::Import(const Byte* orig_moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	if (s_moveset < sizeof(TKMovesetHeader*) || !((TKMovesetHeader*)orig_moveset)->ValidateHeader()) {
		return ImportationErrcode_MovesetDataError;
	}

	Byte* moveset = (Byte*)malloc(s_moveset);
	progress = 5;
	if (moveset == nullptr) {
		return ImportationErrcode_AllocationErr;
	}

	memcpy(moveset, orig_moveset, s_moveset);
	progress = 10;

	ImportationErrcode_ errcode = _Import(moveset, s_moveset, playerAddress, settings, progress);

	free(moveset);
	return errcode;
}

ImportationErrcode_ Importer::_Import(Byte* moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	progress = 15;

	// Header of the moveset that will contain our own information about it
	const TKMovesetHeader* header = (TKMovesetHeader*)moveset;

	bool isCompressed = header->isCompressed();

	if (settings & ImportSettings_ImportOriginalData) {
		const gameAddr gameOriginalMoveset = m_process.allocateMem(s_moveset);
		if (gameOriginalMoveset == 0) {
			return ImportationErrcode_GameAllocationErr;
		}
		m_process.writeBytes(gameOriginalMoveset, moveset, s_moveset);
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

		if (moveset == nullptr) {
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

	if (isCompressed) {
		delete[] moveset;
	}

	return errCode;
}