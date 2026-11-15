#include <string>

#include "helpers.hpp"
#include "Importer_t7.hpp"
#include "MovesetConverters.hpp"
#include "GameIDs.hpp"
#include "T8ToT7.hpp"
#include <exception>
#include <vector>

#include "Structs_t7.h"

using namespace StructsT7;
using namespace T7ImportUtils;

// Contains the same structure as StructsT7 but with gameAddr types instead of ptrs types
// Defined here because i don't want any other file to have access to this shortcut
#define gAddr StructsT7_gameAddr

// -- Private methods -- //

ImportationErrcode_ ImporterT7::_Import_FromT8(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	DEBUG_LOG("_Import_FromT8()\n");
	if (header->minorVersion != GameVersions::T8::RETAIL) {
		m_lastError = "Only retail Tekken 8 movesets convert to Tekken 7";
		return ImportationErrcode_UnsupportedGameVersion;
	}
	// Self-contained conversion (the same T7 file for every target), then the normal T7 import of that file.
	std::vector<Byte> file;
	try {
		file = T8ToT7::Convert(*header, { moveset, static_cast<size_t>(s_moveset) }, nullptr, true, ImportProfile::Read(*header));
	}
	catch (const std::bad_alloc&) {
		m_lastError = "Not enough memory to convert the Tekken 8 moveset";
		return ImportationErrcode_AllocationErr;
	}
	catch (const std::exception& error) {
		m_lastError = std::string("Tekken 8 to Tekken 7 conversion failed: ") + error.what();
		DEBUG_ERR("%s", m_lastError.c_str());
		return ImportationErrcode_MovesetDataError;
	}
	progress = 10;
	auto* converted = reinterpret_cast<const TKMovesetHeader*>(file.data());
	return _Import_FromT7(converted, file.data() + converted->moveset_data_start, file.size() - converted->moveset_data_start,
		playerAddress, settings, progress);
}

ImportationErrcode_ ImporterT7::_Import_FromTTT2(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	DEBUG_LOG("_Import_FromTTT2()\n");
	// Basic load is tied to online importing where we might want to import the moveset but not populate empty MOTA and apply character-specific fixes just yet
	const bool BASIC_LOAD = (settings & ImportSettings_BasicLoadOnly) != 0;

	Byte* orig_moveset = moveset;

	TKMovesetHeaderBlocks t7_offsets;
	if (!MovesetConverter::TTT2ToT7().Convert(header, moveset, s_moveset, t7_offsets)) {

		return ImportationErrcode_AllocationErr;
	}

	// List of data blocks within the moveset
	const TKMovesetHeaderBlocks * offsets = &t7_offsets;
	if (!ValidateT7Semantics(*offsets, moveset, s_moveset)) {
		delete[] moveset;
		return ImportationErrcode_MovesetDataError;
	}

	// Table that contains offsets and amount of cancels, move, requirements, etc...
	gAddr::MovesetTable* table;

	// -- Basic reading & allocations -- //

	progress = 20;

	// Allocate our moveset in the game's memory, but we aren't gonna write on that for a while.
	// The idea is to write on our moveset in our own memory (should be faster), then write it all at once on gameMoveset with a single m_process.writeBytes()
	const gameAddr gameMoveset = m_process.allocateMem(s_moveset);
	if (gameMoveset == 0) {
		delete[] moveset;
		return ImportationErrcode_GameAllocationErr;
	}
	progress = 40;

	// -- Conversions -- //

	// Get the table address
	table = (gAddr::MovesetTable*)(moveset + offsets->tableBlock);

	CorrectMovesetInfoValues((MovesetInfo*)moveset, gameMoveset);

	//Convert move offets into ptrs
	if (!ConvertMovesetIndexes(moveset, gameMoveset, table, offsets)) {
		m_process.freeMem(gameMoveset);
		delete[] moveset;
		return ImportationErrcode_MovesetDataError;
	}
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
	ConvertMotaListOffsets(offsets, moveset, gameMoveset, playerAddress, !BASIC_LOAD, settings);
	progress = 90;

	MvlHead* mvlHead = (MvlHead*)(moveset + offsets->movelistBlock);
	bool hasDisplayableMovelist = (s_moveset - offsets->movelistBlock >= 4) &&
		(strncmp(mvlHead->mvlString, "MVLT", 4) == 0);

	if (hasDisplayableMovelist) {
		// todo
		ConvertDisplayableMovelistOffsets(mvlHead);
	}

	// -- Allocation & Conversion finished -- //

	if (hasDisplayableMovelist && !BASIC_LOAD)
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
	if (!m_process.writeBytes(gameMoveset, moveset, s_moveset)) {
		m_process.freeMem(gameMoveset);
		delete[] moveset;
		return ImportationErrcode_GameAllocationErr;
	}
	progress = 99;
	DEBUG_LOG("-- Imported moveset at %llx --\n", gameMoveset);

	
	if (!BASIC_LOAD) {
		// Then write our moveset address to the current player
		m_process.writeInt64(playerAddress + m_game.GetValue("motbin_offset"), gameMoveset);
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

		return ImportationErrcode_AllocationErr;
	}

	// List of data blocks within the moveset
	const TKMovesetHeaderBlocks* offsets = &t7_offsets;
	if (!ValidateT7Semantics(*offsets, moveset, s_moveset)) {
		delete[] moveset;
		return ImportationErrcode_MovesetDataError;
	}

	// Table that contains offsets and amount of cancels, move, requirements, etc...
	gAddr::MovesetTable* table;

	// -- Basic reading & allocations -- //

	progress = 20;

	// Allocate our moveset in the game's memory, but we aren't gonna write on that for a while.
	// The idea is to write on our moveset in our own memory (should be faster), then write it all at once on gameMoveset with a single m_process.writeBytes()
	const gameAddr gameMoveset = m_process.allocateMem(s_moveset);
	if (gameMoveset == 0) {
		delete[] moveset;
		return ImportationErrcode_GameAllocationErr;
	}
	progress = 40;

	// -- Conversions -- //

	// Get the table address
	table = (gAddr::MovesetTable*)(moveset + offsets->tableBlock);

	CorrectMovesetInfoValues((MovesetInfo*)moveset, gameMoveset);

	//Convert move offets into ptrs
	if (!ConvertMovesetIndexes(moveset, gameMoveset, table, offsets)) {
		m_process.freeMem(gameMoveset);
		delete[] moveset;
		return ImportationErrcode_MovesetDataError;
	}
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
	ConvertMotaListOffsets(offsets, moveset, gameMoveset, playerAddress, !BASIC_LOAD, settings);
	progress = 90;

	MvlHead* mvlHead = (MvlHead*)(moveset + offsets->movelistBlock);
	bool hasDisplayableMovelist = (s_moveset - offsets->movelistBlock >= 4) &&
		(strncmp(mvlHead->mvlString, "MVLT", 4) == 0);

	if (hasDisplayableMovelist) {
		// todo
		ConvertDisplayableMovelistOffsets(mvlHead);
	}

	// -- Allocation & Conversion finished -- //

	if (hasDisplayableMovelist && !BASIC_LOAD)
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
	if (!m_process.writeBytes(gameMoveset, moveset, s_moveset)) {
		m_process.freeMem(gameMoveset);
		delete[] moveset;
		return ImportationErrcode_GameAllocationErr;
	}
	progress = 99;
	DEBUG_LOG("-- Imported moveset at %llx --\n", gameMoveset);


	if (!BASIC_LOAD) {
		// Then write our moveset address to the current player
		m_process.writeInt64(playerAddress + m_game.GetValue("motbin_offset"), gameMoveset);
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

		return ImportationErrcode_AllocationErr;
	}

	// List of data blocks within the moveset
	const TKMovesetHeaderBlocks* offsets = &t7_offsets;
	if (!ValidateT7Semantics(*offsets, moveset, s_moveset)) {
		delete[] moveset;
		return ImportationErrcode_MovesetDataError;
	}

	// Table that contains offsets and amount of cancels, move, requirements, etc...
	gAddr::MovesetTable* table;

	// -- Basic reading & allocations -- //

	progress = 20;

	// Allocate our moveset in the game's memory, but we aren't gonna write on that for a while.
	// The idea is to write on our moveset in our own memory (should be faster), then write it all at once on gameMoveset with a single m_process.writeBytes()
	const gameAddr gameMoveset = m_process.allocateMem(s_moveset);
	if (gameMoveset == 0) {
		delete[] moveset;
		return ImportationErrcode_GameAllocationErr;
	}
	progress = 40;

	// -- Conversions -- //

	// Get the table address
	table = (gAddr::MovesetTable*)(moveset + offsets->tableBlock);

	CorrectMovesetInfoValues((MovesetInfo*)moveset, gameMoveset);

	//Convert move offets into ptrs
	if (!ConvertMovesetIndexes(moveset, gameMoveset, table, offsets)) {
		m_process.freeMem(gameMoveset);
		delete[] moveset;
		return ImportationErrcode_MovesetDataError;
	}
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
	ConvertMotaListOffsets(offsets, moveset, gameMoveset, playerAddress, !BASIC_LOAD, settings);
	progress = 90;

	MvlHead* mvlHead = (MvlHead*)(moveset + offsets->movelistBlock);
	bool hasDisplayableMovelist = (s_moveset - offsets->movelistBlock >= 4) &&
		(strncmp(mvlHead->mvlString, "MVLT", 4) == 0);

	if (hasDisplayableMovelist) {
		// todo
		ConvertDisplayableMovelistOffsets(mvlHead);
	}

	// -- Allocation & Conversion finished -- //

	if (hasDisplayableMovelist && !BASIC_LOAD)
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
	if (!m_process.writeBytes(gameMoveset, moveset, s_moveset)) {
		m_process.freeMem(gameMoveset);
		delete[] moveset;
		return ImportationErrcode_GameAllocationErr;
	}
	progress = 99;
	DEBUG_LOG("-- Imported moveset at %llx --\n", gameMoveset);


	if (!BASIC_LOAD) {
		// Then write our moveset address to the current player
		m_process.writeInt64(playerAddress + m_game.GetValue("motbin_offset"), gameMoveset);
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
	if (header->minorVersion == GameVersions::T5::DR_ONLINE) {
		if (!MovesetConverter::T5DRToT7().Convert(header, moveset, s_moveset, t7_offsets)) {

			return ImportationErrcode_AllocationErr;
		}
	}
	else {
		if (!MovesetConverter::T5ToT7().Convert(header, moveset, s_moveset, t7_offsets)) {

			return ImportationErrcode_AllocationErr;
		}
	}

	// List of data blocks within the moveset
	const TKMovesetHeaderBlocks* offsets = &t7_offsets;
	if (!ValidateT7Semantics(*offsets, moveset, s_moveset)) {
		delete[] moveset;
		return ImportationErrcode_MovesetDataError;
	}

	// Table that contains offsets and amount of cancels, move, requirements, etc...
	gAddr::MovesetTable* table;

	// -- Basic reading & allocations -- //

	progress = 20;

	// Allocate our moveset in the game's memory, but we aren't gonna write on that for a while.
	// The idea is to write on our moveset in our own memory (should be faster), then write it all at once on gameMoveset with a single m_process.writeBytes()
	const gameAddr gameMoveset = m_process.allocateMem(s_moveset);
	if (gameMoveset == 0) {
		delete[] moveset;
		return ImportationErrcode_GameAllocationErr;
	}
	progress = 40;

	// -- Conversions -- //

	// Get the table address
	table = (gAddr::MovesetTable*)(moveset + offsets->tableBlock);

	CorrectMovesetInfoValues((MovesetInfo*)moveset, gameMoveset);

	//Convert move offets into ptrs
	if (!ConvertMovesetIndexes(moveset, gameMoveset, table, offsets)) {
		m_process.freeMem(gameMoveset);
		delete[] moveset;
		return ImportationErrcode_MovesetDataError;
	}
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
	ConvertMotaListOffsets(offsets, moveset, gameMoveset, playerAddress, !BASIC_LOAD, settings);
	progress = 90;

	MvlHead* mvlHead = (MvlHead*)(moveset + offsets->movelistBlock);
	bool hasDisplayableMovelist = (s_moveset - offsets->movelistBlock >= 4) &&
		(strncmp(mvlHead->mvlString, "MVLT", 4) == 0);

	if (hasDisplayableMovelist) {
		// todo
		ConvertDisplayableMovelistOffsets(mvlHead);
	}

	// -- Allocation & Conversion finished -- //

	if (hasDisplayableMovelist && !BASIC_LOAD)
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
	if (!m_process.writeBytes(gameMoveset, moveset, s_moveset)) {
		m_process.freeMem(gameMoveset);
		delete[] moveset;
		return ImportationErrcode_GameAllocationErr;
	}
	progress = 99;
	DEBUG_LOG("-- Imported moveset at %llx --\n", gameMoveset);


	if (!BASIC_LOAD) {
		// Then write our moveset address to the current player
		m_process.writeInt64(playerAddress + m_game.GetValue("motbin_offset"), gameMoveset);
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
