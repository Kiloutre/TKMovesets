#include <string>

#include "helpers.hpp"
#include "Importer_t7.hpp"
#include "GameIDs.hpp"

#include "Structs_t7.h"

using namespace StructsT7;
using namespace T7ImportUtils;

// Contains the same structure as StructsT7 but with gameAddr types instead of ptrs types
// Defined here because i don't want any other file to have access to this shortcut
#define gAddr StructsT7_gameAddr

// -- Private methods -- //


void ImporterT7::ForcePlayerMove(gameAddr playerAddress, gameAddr playerMoveset, size_t moveId)
{
	{
		/// Yes, this needs to be done here, this is only useful when we really want to set the current move
		// Each player actually holds 5 copies of its moveset ptr.
		// One of them is the one to default to when going back to aliases move (>= 32768)
		// One of them is the one of the move that is currently playing, which may come from the opponent's moveset
		gameAddr movesetOffset = playerAddress + m_game->GetValue("motbin_offset");
		for (size_t i = 1; i < 5; ++i) {
			m_process->writeInt64(movesetOffset + i * 8, playerMoveset);
		}
	}

	if (moveId >= 0x8000) {
		// If is alias, convert it to its regular move id thanks to the alias list (uint16_t each) starting at 0x28
		moveId = m_process->readInt16(playerMoveset + offsetof(MovesetInfo, current_aliases) + (0x2 * (moveId - 0x8000)));
	}

	uint64_t movesOffset = offsetof(MovesetInfo, table) + offsetof(MovesetTable, move);
	gameAddr moveAddr = m_process->readInt64(playerMoveset + movesOffset) + moveId * sizeof(Move);

	// Write a big number to the frame timer to force the current move end
	m_process->writeInt32(playerAddress + m_game->GetValue("currmove_timer"), 99999);
	// Tell the game which move to play NEXT
	m_process->writeInt64(playerAddress + m_game->GetValue("nextmove_addr"), moveAddr);
	// Also tell the ID of the current move. This isn't required per se, but not doing that would make the current move ID 0, which i don't like.
	m_process->writeInt64(playerAddress + m_game->GetValue("currmove_id"), moveId);
}


void ImporterT7::WriteCameraMotasToPlayer(gameAddr movesetAddr, gameAddr playerAddress)
{
	const uint64_t staticCameraOffset = m_game->GetValue("static_camera_offset");

	uint64_t motaOffset = offsetof(MovesetInfo, motas);
	gameAddr cameraMota1 = m_process->readInt64(movesetAddr + motaOffset + offsetof(MotaList, camera_1));
	gameAddr cameraMota2 = m_process->readInt64(movesetAddr + motaOffset + offsetof(MotaList, camera_2));

	m_process->writeInt64(playerAddress + staticCameraOffset, cameraMota1);
	m_process->writeInt64(playerAddress + staticCameraOffset + 0x8, cameraMota2);
}


void ImporterT7::ConvertMotaListOffsets(const TKMovesetHeaderBlocks* offsets, Byte* moveset, gameAddr gameMoveset, gameAddr playerAddress, bool replaceEmpty)
{
	MotaList currentMotasList{};
	gameAddr currentMovesetAddr = m_process->readInt64(playerAddress + m_game->GetValue("motbin_offset"));
	m_process->readBytes(currentMovesetAddr + offsetof(MovesetInfo, motas), &currentMotasList, sizeof(MotaList));

	MotaList* motaList = (MotaList*)(moveset + offsets->motalistsBlock);

	uint64_t* gameMotas = (uint64_t*)&currentMotasList;
	uint64_t* fileMotas = (uint64_t*)motaList;

	// This is just a list of uint64_t anyway so might as well do this
	for (size_t i = 0; i <= 12; ++i)
	{
		if (fileMotas[i] != MOVESET_ADDR_MISSING)
		{
			MotaHeader* mota = (MotaHeader*)(offsets->GetBlock(TKMovesetHeaderBlocks_Mota, moveset) + fileMotas[i]);
			if (mota->IsValid()) {
				DEBUG_LOG("Using custom mota %llu\n", i);
				fileMotas[i] += gameMoveset + offsets->motaBlock;
				continue;
			}
		}

		if (replaceEmpty) {
			// Moveset block was not included in the file: copy the currently used one
			fileMotas[i] = gameMotas[i];
		}
	}
}


void ImporterT7::ApplyCharacterIDFixes(Byte* moveset, gameAddr playerAddress, const gAddr::MovesetTable* table, const TKMovesetHeader* header, const TKMovesetHeaderBlocks* offsets)
{
	// In movesets, some moves (for some reason) can be transitionned into only on specific character IDs
	// I am taking about mundane moves such as EWHF not working where WHF does
	// The why is hard to understand and might possibly be linked to Mokujin/Combot, but anyway, this fixes things

	uint16_t movesetCharacterId = header->characterId;
	uint16_t currentCharacterId = m_process->readInt16(playerAddress + m_game->GetValue("chara_id_offset"));

	Requirement* requirement = (Requirement*)(moveset + offsets->movesetBlock + table->requirement);
	const int c_characterIdCondition = (int)m_game->GetValue("character_id_condition");

	for (size_t i = 0; i < table->requirementCount; ++i)
	{
		// When the requirement ask "am i X character ID", X = extracted character ID
		// i will change that X character ID to be the one of the current character, to make it always true.
		// When the requirement ask for any other character ID, i will supply a character ID that ISN'T the current one, to make it always false.

		if (requirement[i].condition == c_characterIdCondition) {
			requirement[i].param_unsigned = requirement[i].param_unsigned == movesetCharacterId ? currentCharacterId : currentCharacterId + 10;
		}
	}

	OtherMoveProperty* prop = (OtherMoveProperty*)(moveset + offsets->movesetBlock + table->moveBeginningProp);
	for (size_t i = 0; i < table->moveBeginningPropCount; ++i)
	{
		if (prop->extraprop == c_characterIdCondition) {
			prop[i].extraprop = requirement[i].param_unsigned == movesetCharacterId ? currentCharacterId : currentCharacterId + 10;
		}
	}

	prop = (OtherMoveProperty*)(moveset + offsets->movesetBlock + table->moveEndingProp);
	for (size_t i = 0; i < table->moveEndingPropCount; ++i)
	{
		if (prop->extraprop == c_characterIdCondition) {
			prop[i].extraprop = requirement[i].param_unsigned == movesetCharacterId ? currentCharacterId : currentCharacterId + 10;
		}
	}
}


void ImporterT7::ImportMovelist(MvlHead* mvlHead, gameAddr game_mlvHead, gameAddr playerAddress)
{
	gameAddr managerAddr = m_game->ReadPtrPath("movelist_manager_addr");

	int playerId = m_process->readInt32(playerAddress + m_game->GetValue("playerid_offset"));

	if (playerId == 1) {
		managerAddr += sizeof(MvlManager);
	}

	m_process->writeInt64(managerAddr + offsetof(MvlManager, mvlHead), game_mlvHead);
	m_process->writeInt64(managerAddr + offsetof(MvlManager, displayableEntriesCount), mvlHead->displayables_count);
	m_process->writeInt64(managerAddr + offsetof(MvlManager, mvlDisplayableBlock), game_mlvHead + mvlHead->displayables_offset);
}


ImportationErrcode_ ImporterT7::_Import_FromT7(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	DEBUG_LOG("_Import_FromT7()\n");
	// Basic load is tied to online importing where we might want to import the moveset but not populate empty MOTA and apply character-specific fixes just yet
	const bool BASIC_LOAD = (settings & ImportSettings_BasicLoadOnly) != 0;

	// List of data blocks within the moveset
	const TKMovesetHeaderBlocks* offsets = (const TKMovesetHeaderBlocks*)((char*)header + header->block_list);

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

	return ImportationErrcode_Successful;
}


// -- Public methods -- //


void ImporterT7::CleanupUnusedMovesets()
{
	gameAddr playerAddress = m_game->ReadPtrPath("p1_addr");
	uint64_t playerstructSize = m_game->GetValue("playerstruct_size");
	uint64_t motbinOffset = m_game->GetValue("motbin_offset");

	/*
	uint64_t offsetsToWatch[] = {
		0x218, 0x220, 0x228, // Move
		0xBC8, 0xD80, 0xDA0, // ???
		0x13C0, 0x13E8, 0x13F0, // ???
		0x14a0, 0x14a8, // Static camera mota
		0x1460, 0x1468, 0x1480, 0x1488,// ??
		0x1520, 0x1528, 0x1530, 0x1540 // Moveset
	};
	*/

	// Call CanImport() because it's a quick way to see if players are loaded.
	// If they're not, we can free memory worry-free.
	if (!CanImport()) {
		for (size_t i = 0; i < m_process->allocatedMemory.size(); ++i) {
			std::pair<gameAddr, uint64_t> block = m_process->allocatedMemory[i];
			gameAddr movesetAddress = block.first;
			m_process->freeMem(movesetAddress);
		}
		return;
	}

	// Check mvl manager
	MvlManager mvlManager[2];
	uint64_t mvlManagerAddr = m_game->ReadPtrPath("movelist_manager_addr");
	if (mvlManagerAddr != 0) {
		m_process->readBytes(mvlManagerAddr, mvlManager, sizeof(mvlManager));
	}
	else {
		memset(mvlManager, 0, sizeof(mvlManager));
		DEBUG_LOG("::CleanupUnusedMovesets() : MVL Manager ptr path empty\n");
	}

	for (size_t i = 0; i + 1 < m_process->allocatedMemory.size();)
	{
		std::pair<gameAddr, uint64_t> block = m_process->allocatedMemory[i];
		gameAddr movesetAddress = block.first;
		uint64_t movesetEnd = movesetAddress + block.second;
		bool isUsed = false;

		for (int j = 0; j < 2; ++j) {
			if (
				(movesetAddress <= (gameAddr)mvlManager[j].mvlHead && (gameAddr)mvlManager[j].mvlHead <= movesetEnd)
				|| (movesetAddress <= (gameAddr)mvlManager[j].sequenceEnd && (gameAddr)mvlManager[j].sequenceEnd <= movesetEnd)
				|| (movesetAddress <= (gameAddr)mvlManager[j].sequenceStart && (gameAddr)mvlManager[j].sequenceStart <= movesetEnd)
				) {
				isUsed = true;
				break;
			}
		}

		// Check movesets of both players
		for (size_t playerid = 0; playerid < 2 && !isUsed; ++playerid)
		{
			gameAddr currentPlayerAddress = playerAddress + playerid * playerstructSize;

			// Maybe overkill to check every offset, but drastically reduces the chance of a crash, and isn't that slow
			gameAddr pStart = currentPlayerAddress;
			gameAddr pEnd = pStart + playerstructSize;
			while (pStart < pEnd)
			{
				uint64_t offsetValue = m_process->readUInt64(pStart);
				if (movesetAddress <= offsetValue && offsetValue < movesetEnd) {
					isUsed = true;
					break;
				}
				pStart += 8;
			}

		}


		if (isUsed) {
			// Skip
			++i;
		}
		else {
			m_process->freeMem(movesetAddress);
		}

	}
}


bool ImporterT7::CanImport()
{
	// todo: this is invalid, because when we import our own moveset and leave back to main menu, it will return true
	// yes we can import in that case but it will serve zero purpose
	gameAddr playerAddress = m_game->ReadPtrPath("p1_addr");
	// We'll just read through a bunch of values that wouldn't be valid if a moveset wasn't loaded
	// readInt64() may return -1 if the read fails so we have to check for this value as well.

	if (playerAddress == 0 || playerAddress == -1) {
		return false;
	}

	for (int i = 0; i < 2; ++i)
	{
		gameAddr player = playerAddress + i * m_game->GetValue("playerstruct_size");
		gameAddr currentMove = m_process->readInt64(player + m_game->GetValue("currmove"));
		if (currentMove == 0 || currentMove == -1) {
			return false;
		}

		gameAddr animAddr = m_process->readInt64(currentMove + 0x10);
		if (animAddr == 0 || animAddr == -1) {
			return false;
		}

		uint8_t animType = m_process->readInt8(animAddr);
		if (animType != 0x64 && animType != 0xC8) {
			return false;
		}
	}
	return true;
}


gameAddr ImporterT7::GetCharacterAddress(uint8_t playerId)
{
	gameAddr playerAddress = m_game->ReadPtrPath("p1_addr");
	if (playerId > 0) {
		playerAddress += playerId * m_game->GetValue("playerstruct_size");
	}
	return playerAddress;
}


gameAddr ImporterT7::GetMovesetAddress(uint8_t playerId)
{
	gameAddr playerAddress = m_game->ReadPtrPath("p1_addr");
	if (playerAddress > 0) {
		playerAddress += playerId * m_game->GetValue("playerstruct_size");
		return m_process->readInt64(playerAddress + m_game->GetValue("motbin_offset"));
	}
	return 0;
}

ImportationErrcode_ ImporterT7::ImportMovesetData(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	auto gameId = header->gameId;
	auto minorVersion = header->minorVersion;

	ImportationErrcode_(ImporterT7::*importFunc)( const TKMovesetHeader * header, Byte * moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t & progress);

	switch (gameId)
	{
	case GameId_T7:
		importFunc = &ImporterT7::_Import_FromT7;
		break;
	case GameId_TTT2:
		importFunc = &ImporterT7::_Import_FromTTT2;
		break;
	case GameId_TREV:
		importFunc = &ImporterT7::_Import_FromTREV;
		break;
	case GameId_T6:
		importFunc = &ImporterT7::_Import_FromT6;
		break;
	case GameId_T5:
		importFunc = &ImporterT7::_Import_FromT5;
		break;
	default:
		DEBUG_ERR("IMPORT: Unsupported game version %u", gameId);
		return ImportationErrcode_UnsupportedGameVersion;
	}

	return (*this.*importFunc)(header, moveset, s_moveset, playerAddress, settings, progress);
}