#include <string>

#include "helpers.hpp"
#include "Importer_t7.hpp"
#include "MovesetConverters.hpp"

#include "Structs_t7.h"

using namespace StructsT7;
using namespace T7ImportUtils;

// Contains the same structure as StructsT7 but with gameAddr types instead of ptrs types
// Defined here because i don't want any other file to have access to this shortcut
#define gAddr StructsT7_gameAddr

// -- Private methods -- //

ImportationErrcode_ ImporterT7::_Import_FromTTT2(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	DEBUG_LOG("_Import_FromTTT2()\n");
	// Basic load is tied to online importing where we might want to import the moveset but not populate empty MOTA and apply character-specific fixes just yet
	const bool BASIC_LOAD = (settings & ImportSettings_BasicLoadOnly) != 0;

	Byte* orig_moveset = moveset;

	TKMovesetHeaderBlocks t7_offsets;
	if (!MovesetConverter::TTT2ToT7().Convert(header, moveset, s_moveset, t7_offsets)) {
		delete[] moveset;
		return ImportationErrcode_AllocationErr;
	}

	// List of data blocks within the moveset
	const TKMovesetHeaderBlocks * offsets = &t7_offsets;

	// Table that contains offsets and amount of cancels, move, requirements, etc...
	gAddr::MovesetTable* table;

	// -- Basic reading & allocations -- //

	progress = 20;

	// Allocate our moveset in the game's memory, but we aren't gonna write on that for a while.
	// The idea is to write on our moveset in our own memory (should be faster), then write it all at once on gameMoveset with a single m_process->writeBytes()
	const gameAddr gameMoveset = m_process->allocateMem(s_moveset);
	if (gameMoveset == 0) {
		return ImportationErrcode_GameAllocationErr;
	}
	progress = 40;

	// -- Conversions -- //

	// Get the table address
	table = (gAddr::MovesetTable*)(moveset + offsets->tableBlock);

	CorrectMovesetInfoValues((MovesetInfo*)moveset, gameMoveset);

	//Convert move offets into ptrs
	ConvertMovesetIndexes(moveset, gameMoveset, table, offsets);
	progress = 70;

	if (!BASIC_LOAD) {
		// Fix moves that use characterID conditions to work
		// todo
		ApplyCharacterIDFixes(moveset, playerAddress, table, header, offsets);
	}
	progress = 85;

	// Turn our table offsets into ptrs. Do this only at the end because we actually need those offsets above
	ConvertMovesetTableOffsets(offsets, moveset, gameMoveset);
	progress = 80;

	// Turn our mota offsets into mota ptrs, or copy the currently loaded character's mota for each we didn't provide
	ConvertMotaListOffsets(offsets, moveset, gameMoveset, playerAddress, !BASIC_LOAD);
	progress = 90;

	MvlHead* mvlHead = (MvlHead*)(moveset + offsets->movelistBlock);
	bool hasDisplayableMovelist = ((header->moveset_data_start + offsets->movelistBlock + 4) < s_moveset) &&
		(strncmp(mvlHead->mvlString, "MVLT", 4) == 0);

	if (hasDisplayableMovelist) {
		// todo
		ConvertDisplayableMovelistOffsets(mvlHead);
	}

	// -- Allocation & Conversion finished -- //

	if (hasDisplayableMovelist)
	{
		gameAddr game_mvlHead = (gameAddr)gameMoveset + offsets->movelistBlock;
		ImportMovelist(mvlHead, game_mvlHead, playerAddress);
	}

	if (settings & ImportSettings_EnforceCurrentAliasesAsDefault) {
		EnforceCurrentAliasesAsDefault(moveset);
	}
	else {
		EnforceDefaultAliasesAsCurrent(moveset);
	}

	// Finally write our moveset to the game's memory
	m_process->writeBytes(gameMoveset, moveset, s_moveset);
	progress = 99;
	DEBUG_LOG("-- Imported moveset at %llx --\n", gameMoveset);

	
	if (!BASIC_LOAD) {
		// Then write our moveset address to the current player
		m_process->writeInt64(playerAddress + m_game->GetValue("motbin_offset"), gameMoveset);
	}
	
	progress = 100;

	
	if (!BASIC_LOAD) {
		// Also write camera mota offsts to the player structure if those motas have been exported
		WriteCameraMotasToPlayer(gameMoveset, playerAddress);
	}
	
	
	if (!BASIC_LOAD) {
		if (settings & ImportSettings_ApplyInstantly) {
			ForcePlayerMove(playerAddress, gameMoveset, 32769);
		}
	}
	

	lastLoaded.crc32 = header->crc32;
	lastLoaded.charId = header->characterId;
	lastLoaded.address = gameMoveset;
	lastLoaded.size = s_moveset;

	delete[] moveset;

	return ImportationErrcode_Successful;
}

ImportationErrcode_ ImporterT7::_Import_FromTREV(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	DEBUG_LOG("_Import_FromTTT2()\n");
	// Basic load is tied to online importing where we might want to import the moveset but not populate empty MOTA and apply character-specific fixes just yet
	const bool BASIC_LOAD = (settings & ImportSettings_BasicLoadOnly) != 0;

	Byte* orig_moveset = moveset;

	TKMovesetHeaderBlocks t7_offsets;
	if (!MovesetConverter::TREVToT7().Convert(header, moveset, s_moveset, t7_offsets)) {
		delete[] moveset;
		return ImportationErrcode_AllocationErr;
	}

	// List of data blocks within the moveset
	const TKMovesetHeaderBlocks* offsets = &t7_offsets;

	// Table that contains offsets and amount of cancels, move, requirements, etc...
	gAddr::MovesetTable* table;

	// -- Basic reading & allocations -- //

	progress = 20;

	// Allocate our moveset in the game's memory, but we aren't gonna write on that for a while.
	// The idea is to write on our moveset in our own memory (should be faster), then write it all at once on gameMoveset with a single m_process->writeBytes()
	const gameAddr gameMoveset = m_process->allocateMem(s_moveset);
	if (gameMoveset == 0) {
		return ImportationErrcode_GameAllocationErr;
	}
	progress = 40;

	// -- Conversions -- //

	// Get the table address
	table = (gAddr::MovesetTable*)(moveset + offsets->tableBlock);

	CorrectMovesetInfoValues((MovesetInfo*)moveset, gameMoveset);

	//Convert move offets into ptrs
	ConvertMovesetIndexes(moveset, gameMoveset, table, offsets);
	progress = 70;

	if (!BASIC_LOAD) {
		// Fix moves that use characterID conditions to work
		// todo
		ApplyCharacterIDFixes(moveset, playerAddress, table, header, offsets);
	}
	progress = 85;

	// Turn our table offsets into ptrs. Do this only at the end because we actually need those offsets above
	ConvertMovesetTableOffsets(offsets, moveset, gameMoveset);
	progress = 80;

	// Turn our mota offsets into mota ptrs, or copy the currently loaded character's mota for each we didn't provide
	ConvertMotaListOffsets(offsets, moveset, gameMoveset, playerAddress, !BASIC_LOAD);
	progress = 90;

	MvlHead* mvlHead = (MvlHead*)(moveset + offsets->movelistBlock);
	bool hasDisplayableMovelist = ((header->moveset_data_start + offsets->movelistBlock + 4) < s_moveset) &&
		(strncmp(mvlHead->mvlString, "MVLT", 4) == 0);

	if (hasDisplayableMovelist) {
		// todo
		ConvertDisplayableMovelistOffsets(mvlHead);
	}

	// -- Allocation & Conversion finished -- //

	if (hasDisplayableMovelist)
	{
		gameAddr game_mvlHead = (gameAddr)gameMoveset + offsets->movelistBlock;
		ImportMovelist(mvlHead, game_mvlHead, playerAddress);
	}

	if (settings & ImportSettings_EnforceCurrentAliasesAsDefault) {
		EnforceCurrentAliasesAsDefault(moveset);
	}
	else {
		EnforceDefaultAliasesAsCurrent(moveset);
	}

	// Finally write our moveset to the game's memory
	m_process->writeBytes(gameMoveset, moveset, s_moveset);
	progress = 99;
	DEBUG_LOG("-- Imported moveset at %llx --\n", gameMoveset);


	if (!BASIC_LOAD) {
		// Then write our moveset address to the current player
		m_process->writeInt64(playerAddress + m_game->GetValue("motbin_offset"), gameMoveset);
	}

	progress = 100;


	if (!BASIC_LOAD) {
		// Also write camera mota offsts to the player structure if those motas have been exported
		WriteCameraMotasToPlayer(gameMoveset, playerAddress);
	}


	if (!BASIC_LOAD) {
		if (settings & ImportSettings_ApplyInstantly) {
			ForcePlayerMove(playerAddress, gameMoveset, 32769);
		}
	}


	lastLoaded.crc32 = header->crc32;
	lastLoaded.charId = header->characterId;
	lastLoaded.address = gameMoveset;
	lastLoaded.size = s_moveset;

	delete[] moveset;

	return ImportationErrcode_Successful;
}


ImportationErrcode_ ImporterT7::_Import_FromT6(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	DEBUG_LOG("_Import_FromT6()\n");
	// Basic load is tied to online importing where we might want to import the moveset but not populate empty MOTA and apply character-specific fixes just yet
	const bool BASIC_LOAD = (settings & ImportSettings_BasicLoadOnly) != 0;

	Byte* orig_moveset = moveset;

	TKMovesetHeaderBlocks t7_offsets;
	if (!MovesetConverter::T6ToT7().Convert(header, moveset, s_moveset, t7_offsets)) {
		delete[] moveset;
		return ImportationErrcode_AllocationErr;
	}

	// List of data blocks within the moveset
	const TKMovesetHeaderBlocks* offsets = &t7_offsets;

	// Table that contains offsets and amount of cancels, move, requirements, etc...
	gAddr::MovesetTable* table;

	// -- Basic reading & allocations -- //

	progress = 20;

	// Allocate our moveset in the game's memory, but we aren't gonna write on that for a while.
	// The idea is to write on our moveset in our own memory (should be faster), then write it all at once on gameMoveset with a single m_process->writeBytes()
	const gameAddr gameMoveset = m_process->allocateMem(s_moveset);
	if (gameMoveset == 0) {
		return ImportationErrcode_GameAllocationErr;
	}
	progress = 40;

	// -- Conversions -- //

	// Get the table address
	table = (gAddr::MovesetTable*)(moveset + offsets->tableBlock);

	CorrectMovesetInfoValues((MovesetInfo*)moveset, gameMoveset);

	//Convert move offets into ptrs
	ConvertMovesetIndexes(moveset, gameMoveset, table, offsets);
	progress = 70;

	if (!BASIC_LOAD) {
		// Fix moves that use characterID conditions to work
		// todo
		ApplyCharacterIDFixes(moveset, playerAddress, table, header, offsets);
	}
	progress = 85;

	// Turn our table offsets into ptrs. Do this only at the end because we actually need those offsets above
	ConvertMovesetTableOffsets(offsets, moveset, gameMoveset);
	progress = 80;

	// Turn our mota offsets into mota ptrs, or copy the currently loaded character's mota for each we didn't provide
	ConvertMotaListOffsets(offsets, moveset, gameMoveset, playerAddress, !BASIC_LOAD);
	progress = 90;

	MvlHead* mvlHead = (MvlHead*)(moveset + offsets->movelistBlock);
	bool hasDisplayableMovelist = ((header->moveset_data_start + offsets->movelistBlock + 4) < s_moveset) &&
		(strncmp(mvlHead->mvlString, "MVLT", 4) == 0);

	if (hasDisplayableMovelist) {
		// todo
		ConvertDisplayableMovelistOffsets(mvlHead);
	}

	// -- Allocation & Conversion finished -- //

	if (hasDisplayableMovelist)
	{
		gameAddr game_mvlHead = (gameAddr)gameMoveset + offsets->movelistBlock;
		ImportMovelist(mvlHead, game_mvlHead, playerAddress);
	}

	if (settings & ImportSettings_EnforceCurrentAliasesAsDefault) {
		EnforceCurrentAliasesAsDefault(moveset);
	}
	else {
		EnforceDefaultAliasesAsCurrent(moveset);
	}

	// Finally write our moveset to the game's memory
	m_process->writeBytes(gameMoveset, moveset, s_moveset);
	progress = 99;
	DEBUG_LOG("-- Imported moveset at %llx --\n", gameMoveset);


	if (!BASIC_LOAD) {
		// Then write our moveset address to the current player
		m_process->writeInt64(playerAddress + m_game->GetValue("motbin_offset"), gameMoveset);
	}

	progress = 100;


	if (!BASIC_LOAD) {
		// Also write camera mota offsts to the player structure if those motas have been exported
		WriteCameraMotasToPlayer(gameMoveset, playerAddress);
	}


	if (!BASIC_LOAD) {
		if (settings & ImportSettings_ApplyInstantly) {
			ForcePlayerMove(playerAddress, gameMoveset, 32769);
		}
	}


	lastLoaded.crc32 = header->crc32;
	lastLoaded.charId = header->characterId;
	lastLoaded.address = gameMoveset;
	lastLoaded.size = s_moveset;

	delete[] moveset;

	return ImportationErrcode_Successful;
}


ImportationErrcode_ ImporterT7::_Import_FromT5(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	DEBUG_LOG("_Import_FromT5()\n");
	// Basic load is tied to online importing where we might want to import the moveset but not populate empty MOTA and apply character-specific fixes just yet
	const bool BASIC_LOAD = (settings & ImportSettings_BasicLoadOnly) != 0;

	Byte* orig_moveset = moveset;

	TKMovesetHeaderBlocks t7_offsets;
	if (!MovesetConverter::T5ToT7().Convert(header, moveset, s_moveset, t7_offsets)) {
		delete[] moveset;
		return ImportationErrcode_AllocationErr;
	}

	// List of data blocks within the moveset
	const TKMovesetHeaderBlocks* offsets = &t7_offsets;

	// Table that contains offsets and amount of cancels, move, requirements, etc...
	gAddr::MovesetTable* table;

	// -- Basic reading & allocations -- //

	progress = 20;

	// Allocate our moveset in the game's memory, but we aren't gonna write on that for a while.
	// The idea is to write on our moveset in our own memory (should be faster), then write it all at once on gameMoveset with a single m_process->writeBytes()
	const gameAddr gameMoveset = m_process->allocateMem(s_moveset);
	if (gameMoveset == 0) {
		return ImportationErrcode_GameAllocationErr;
	}
	progress = 40;

	// -- Conversions -- //

	// Get the table address
	table = (gAddr::MovesetTable*)(moveset + offsets->tableBlock);

	CorrectMovesetInfoValues((MovesetInfo*)moveset, gameMoveset);

	//Convert move offets into ptrs
	ConvertMovesetIndexes(moveset, gameMoveset, table, offsets);
	progress = 70;

	if (!BASIC_LOAD) {
		// Fix moves that use characterID conditions to work
		// todo
		ApplyCharacterIDFixes(moveset, playerAddress, table, header, offsets);
	}
	progress = 85;

	// Turn our table offsets into ptrs. Do this only at the end because we actually need those offsets above
	ConvertMovesetTableOffsets(offsets, moveset, gameMoveset);
	progress = 80;

	// Turn our mota offsets into mota ptrs, or copy the currently loaded character's mota for each we didn't provide
	ConvertMotaListOffsets(offsets, moveset, gameMoveset, playerAddress, !BASIC_LOAD);
	progress = 90;

	MvlHead* mvlHead = (MvlHead*)(moveset + offsets->movelistBlock);
	bool hasDisplayableMovelist = ((header->moveset_data_start + offsets->movelistBlock + 4) < s_moveset) &&
		(strncmp(mvlHead->mvlString, "MVLT", 4) == 0);

	if (hasDisplayableMovelist) {
		// todo
		ConvertDisplayableMovelistOffsets(mvlHead);
	}

	// -- Allocation & Conversion finished -- //

	if (hasDisplayableMovelist)
	{
		gameAddr game_mvlHead = (gameAddr)gameMoveset + offsets->movelistBlock;
		ImportMovelist(mvlHead, game_mvlHead, playerAddress);
	}

	if (settings & ImportSettings_EnforceCurrentAliasesAsDefault) {
		EnforceCurrentAliasesAsDefault(moveset);
	}
	else {
		EnforceDefaultAliasesAsCurrent(moveset);
	}

	// Finally write our moveset to the game's memory
	m_process->writeBytes(gameMoveset, moveset, s_moveset);
	progress = 99;
	DEBUG_LOG("-- Imported moveset at %llx --\n", gameMoveset);


	if (!BASIC_LOAD) {
		// Then write our moveset address to the current player
		m_process->writeInt64(playerAddress + m_game->GetValue("motbin_offset"), gameMoveset);
	}

	progress = 100;


	if (!BASIC_LOAD) {
		// Also write camera mota offsts to the player structure if those motas have been exported
		WriteCameraMotasToPlayer(gameMoveset, playerAddress);
	}


	if (!BASIC_LOAD) {
		if (settings & ImportSettings_ApplyInstantly) {
			ForcePlayerMove(playerAddress, gameMoveset, 32769);
		}
	}


	lastLoaded.crc32 = header->crc32;
	lastLoaded.charId = header->characterId;
	lastLoaded.address = gameMoveset;
	lastLoaded.size = s_moveset;

	delete[] moveset;

	return ImportationErrcode_Successful;
}
