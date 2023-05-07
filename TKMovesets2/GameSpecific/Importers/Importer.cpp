#include "Importer.hpp"

#include "MovesetStructs.h"

// -- Static helpers -- //

// Reads the the moveset size, allocate the moveset in our own memory and write to it
static Byte* getMovesetInfos(std::ifstream& file, uint64_t& size_out)
{
	file.seekg(0, std::ios::end);
	size_out = file.tellg();
	Byte* moveset = (Byte*)malloc(size_out);
	if (moveset != nullptr) {
		file.seekg(0, std::ios::beg);
		file.read((char*)moveset, size_out);
	}
	file.close();
	return moveset;
}

// -- Public -- //

ImportationErrcode_ Importer::Import(const wchar_t* filename, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	progress = 0;
	// Read file data

	DEBUG_LOG("Loading file '%S'\n", filename);
	std::ifstream file(filename, std::ios::binary);

	if (file.fail()) {
		return ImportationErrcode_FileReadErr;
	}

	// Variables that will store the moveset size & the moveset itself in our own memory
	uint64_t s_moveset;
	Byte* moveset;

	// Allocate a copy of the moveset locally. This is NOT in the game's memory
	moveset = getMovesetInfos(file, s_moveset);
	if (moveset == nullptr) {
		return ImportationErrcode_AllocationErr;
	}

	ImportationErrcode_ errcode = _Import(moveset, s_moveset, playerAddress, settings, progress);

	free(moveset);
	return errcode;
}

ImportationErrcode_ Importer::Import(const Byte* orig_moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	Byte* moveset = (Byte*)malloc(s_moveset);
	if (moveset == nullptr) {
		return ImportationErrcode_AllocationErr;
	}

	memcpy(moveset, orig_moveset, s_moveset);

	ImportationErrcode_ errcode = _Import(moveset, s_moveset, playerAddress, settings, progress);

	free(moveset);
	return errcode;
}