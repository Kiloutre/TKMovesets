#include <string>

#include "helpers.hpp"
#include "Compression.hpp"
#include "MovesetConverters/MovesetConverters.hpp"
#include "Importer_t7/Importer_t7_Utils.hpp"

#include "GameIDs.hpp"
#include "MovesetLoader_t7.hpp"

#include "Structs_t7.h"

using namespace StructsT7;
using namespace T7ImportUtils;

// Contains the same structure as StructsT7 but with gameAddr types instead of ptrs types
// Defined here because i don't want any other file to have access to this shortcut
#define gAddr StructsT7_gameAddr

// Turns an index into an absolute address, or NULL (0) if index is -1
# define FROM_INDEX(field, listStartAddr, type) (field = (field == -1 ? 0 : listStartAddr + (field * sizeof(type))))

// -- -- //


static void ConvertMotaListOffsets(const TKMovesetHeaderBlocks* offsets, Byte* moveset, gameAddr gameMoveset)
{
	MotaList currentMotasList{};

	MotaList* motaList = (MotaList*)(moveset + offsets->motalistsBlock);

	uint64_t* gameMotas = (uint64_t*)&currentMotasList;
	uint64_t* fileMotas = (uint64_t*)motaList;

	// This is just a list of uint64_t anyway so might as well do this
	for (size_t i = 0; i <= 12; ++i)
	{
		if (fileMotas[i] != MOVESET_ADDR_MISSING) {
			fileMotas[i] += gameMoveset + offsets->motaBlock;
		}
	}
}

// -- -- //

Byte* MovesetLoaderT7::ImportForOnline(SharedMemT7_Player& player, Byte* moveset, uint64_t s_moveset)
{
	Byte* orig_moveset = moveset;

	// Header of the moveset that will contain our own information about it
	const TKMovesetHeader* header = (TKMovesetHeader*)moveset;

	if (s_moveset < sizeof(header) || !header->ValidateHeader()) {
		DEBUG_LOG("Failed to validate header of incoming online moveset.\n");
		delete[] moveset;
		return nullptr;
	}

	bool isCompressed = header->isCompressed();
	auto gameId = header->gameId;

	if (isCompressed) {
		uint64_t src_size = s_moveset - header->moveset_data_start;

		moveset = CompressionUtils::RAW::Moveset::Decompress(moveset, src_size, s_moveset);

		if (moveset == nullptr) {
			DEBUG_LOG("Decompression of moveset failed!\n");
			delete[] orig_moveset;
			return nullptr;
		}
	}
	else {
		// Not compressed
		// Go past the header now that we have a ptr to the header. This will be what is sent to the game.
		s_moveset -= header->moveset_data_start;
		moveset += header->moveset_data_start;
	}

	player.crc32 = header->crc32;
	player.moveset_character_id = header->characterId;
	player.previous_character_id = header->characterId;

	Byte* final_moveset = nullptr;

	switch (gameId)
	{
	case GameId_T7:
		final_moveset = ImportForOnline_FromT7(header, moveset, s_moveset);
		break;
	case GameId_TTT2:
		final_moveset = ImportForOnline_FromTTT2(header, moveset, s_moveset);
		break;
	default:
		DEBUG_LOG("No game found for given game id '%u'\n", gameId);
		break;
	}

	if (isCompressed) {
		delete[] orig_moveset;
	}

	if (final_moveset != nullptr && final_moveset != moveset) {
		delete[] moveset;
	}
	else if (final_moveset == nullptr) {
		DEBUG_LOG("ImportForOnline: Result was zero.\n");
		delete[] moveset;
		return nullptr;

	}

	return final_moveset;
}


Byte* MovesetLoaderT7::ImportForOnline_FromT7(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset)
{
	DEBUG_LOG("ImportForOnline_FromT7()\n");

	// List of data blocks within the moveset
	const TKMovesetHeaderBlocks* offsets = (const TKMovesetHeaderBlocks*)((char*)header + header->block_list);

	// Table that contains offsets and amount of cancels, move, requirements, etc...
	gAddr::MovesetTable* table;

	// TODO: Validate moveset

	// -- Basic reading & allocations -- //

	const gameAddr gameMoveset = (gameAddr)moveset;

	// -- Conversions -- //

	// Get the table address
	table = (gAddr::MovesetTable*)(moveset + offsets->tableBlock);

	CorrectMovesetInfoValues((MovesetInfo*)moveset, gameMoveset);

	//Convert move offets into ptrs
	ConvertMovesetIndexes(moveset, gameMoveset, table, offsets);

	// Turn our table offsets into ptrs. Do this only at the end because we actually need those offsets above
	ConvertMovesetTableOffsets(offsets, moveset, gameMoveset);

	// Turn our mota offsets into mota ptrs, or copy the currently loaded character's mota for each we didn't provide
	ConvertMotaListOffsets(offsets, moveset, gameMoveset);

	// -- Allocation & Conversion finished -- //

	EnforceDefaultAliasesAsCurrent(moveset);

	DEBUG_LOG("-- Imported moveset at %llx --\n", gameMoveset);

	return moveset;
}


Byte* MovesetLoaderT7::ImportForOnline_FromTTT2(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset)
{
	DEBUG_LOG("ImportForOnline_FromTTT2()\n");

	TKMovesetHeaderBlocks t7_offsets;
	MovesetConverter::TTT2ToT7(header, moveset, s_moveset, t7_offsets);

	// List of data blocks within the moveset
	const TKMovesetHeaderBlocks* offsets = &t7_offsets;

	// Table that contains offsets and amount of cancels, move, requirements, etc...
	gAddr::MovesetTable* table;

	// TODO: Validate moveset

	// -- Basic reading & allocations -- //

	const gameAddr gameMoveset = (gameAddr)moveset;

	// -- Conversions -- //

	// Get the table address
	table = (gAddr::MovesetTable*)(moveset + offsets->tableBlock);

	CorrectMovesetInfoValues((MovesetInfo*)moveset, gameMoveset);

	//Convert move offets into ptrs
	ConvertMovesetIndexes(moveset, gameMoveset, table, offsets);

	// Turn our table offsets into ptrs. Do this only at the end because we actually need those offsets above
	ConvertMovesetTableOffsets(offsets, moveset, gameMoveset);

	// Turn our mota offsets into mota ptrs, or copy the currently loaded character's mota for each we didn't provide
	ConvertMotaListOffsets(offsets, moveset, gameMoveset);

	// -- Allocation & Conversion finished -- //

	EnforceDefaultAliasesAsCurrent(moveset);

	DEBUG_LOG("-- Imported moveset at %llx --\n", gameMoveset);

	return moveset;
}