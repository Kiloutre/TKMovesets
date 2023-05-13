#include <string>

#include "helpers.hpp"
#include "Compression.hpp"
#include "Importer_t7.hpp"

#include "Structs_t7.h"
#include "GameIDs.hpp"

using namespace StructsT7;

// Contains the same structure as StructsT7 but with gameAddr types instead of ptrs types
// Defined here because i don't want any other file to have access to this shortcut
#define gAddr StructsT7_gameAddr

// -- -- //

static void ConvertDisplayableMovelistOffsets(MvlHead* mvlHead)
{
	MvlDisplayable* displayable = (MvlDisplayable*)((uint64_t)mvlHead + mvlHead->displayables_offset);
	for (size_t i = 0; i < mvlHead->displayables_count; ++i)
	{
		int32_t absoluteDisplayableOffset = mvlHead->displayables_offset + (int32_t)(i * sizeof(MvlDisplayable));
		for (int j = 0; j < _countof(displayable->all_translation_offsets); ++j) {
			int32_t correctedOffset = displayable->all_translation_offsets[j] - absoluteDisplayableOffset;
			displayable->all_translation_offsets[j] = correctedOffset;
		}
		++displayable;
	}

	MvlPlayable* playable = (MvlPlayable*)((uint64_t)mvlHead + mvlHead->playables_offset);
	for (size_t i = 0; i < mvlHead->playables_count; ++i)
	{
		uint32_t playable_addr = mvlHead->playables_offset + (int32_t)(sizeof(MvlPlayable) * i);
		uint32_t input_sequence_id = playable->input_sequence_offset;
		uint32_t input_sequence_addr = input_sequence_id * sizeof(MvlInput) + mvlHead->inputs_offset;

		playable->input_sequence_offset = input_sequence_addr - playable_addr;
		++playable;
	}
}


// In our locally allocated movest, correct the lists pointing to the various moveset structure lists
static void ConvertMovesetTableOffsets(const TKMovesetHeaderBlocks* offsets, Byte* moveset, gameAddr gameMoveset)
{
	gAddr::MovesetTable* table = (gAddr::MovesetTable*)(moveset + offsets->tableBlock);
	gameAddr offset = gameMoveset + offsets->movesetBlock;

	table->reactions += offset;
	table->requirement += offset;
	table->hitCondition += offset;
	table->projectile += offset;
	table->pushback += offset;
	table->pushbackExtradata += offset;
	table->cancel += offset;
	table->groupCancel += offset;
	table->cancelExtradata += offset;
	table->extraMoveProperty += offset;
	table->moveBeginningProp += offset;
	table->moveEndingProp += offset;
	table->move += offset;
	table->voiceclip += offset;
	table->inputSequence += offset;
	table->input += offset;
	table->unknownParryRelated += offset;
	table->cameraData += offset;
	table->throwCameras += offset;
}


//  Convert indexes of moves, cancels, hit conditions, etc... into ptrs
static void ConvertMovesetIndexes(Byte* moveset, gameAddr gameMoveset, const gAddr::MovesetTable* table, const TKMovesetHeaderBlocks* offsets)
{
	gameAddr blockOffset = gameMoveset + offsets->movesetBlock;

	for (auto& move : StructIterator<gAddr::Move>(moveset, offsets->movesetBlock + table->move, table->moveCount))
	{
		move.name_addr += gameMoveset + offsets->nameBlock;
		move.anim_name_addr += gameMoveset + offsets->nameBlock;
		move.anim_addr += gameMoveset + offsets->animationBlock;

		FROM_INDEX(move.cancel_addr, blockOffset + table->cancel, Cancel);
		FROM_INDEX(move._0x28_cancel_addr, blockOffset + table->cancel, Cancel);
		FROM_INDEX(move._0x38_cancel_addr, blockOffset + table->cancel, Cancel);
		FROM_INDEX(move._0x48_cancel_addr, blockOffset + table->cancel, Cancel);
		FROM_INDEX(move.hit_condition_addr, blockOffset + table->hitCondition, HitCondition);
		FROM_INDEX(move.voicelip_addr, blockOffset + table->voiceclip, Voiceclip);
		FROM_INDEX(move.extra_move_property_addr, blockOffset + table->extraMoveProperty, ExtraMoveProperty);
		FROM_INDEX(move.move_start_extraprop_addr, blockOffset + table->moveBeginningProp, OtherMoveProperty);
		FROM_INDEX(move.move_end_extraprop_addr, blockOffset + table->moveEndingProp, OtherMoveProperty);
	}

	// Convert projectile ptrs
	for (auto& projectile : StructIterator<gAddr::Projectile>(moveset, offsets->movesetBlock + table->projectile, table->projectileCount))
	{
		// One projectile actually has both at NULL for some reason ? todo : check
		FROM_INDEX(projectile.cancel_addr, blockOffset + table->cancel, Cancel);
		FROM_INDEX(projectile.hit_condition_addr, blockOffset + table->hitCondition, HitCondition);
	}

	// Convert cancel ptrs
	for (auto& cancel : StructIterator<gAddr::Cancel>(moveset, offsets->movesetBlock + table->cancel, table->cancelCount))
	{
		FROM_INDEX(cancel.requirements_addr, blockOffset + table->requirement, Requirement);
		FROM_INDEX(cancel.extradata_addr, blockOffset + table->cancelExtradata, CancelExtradata);
	}

	// Convert groupe dcancel ptrs
	for (auto& groupCancel : StructIterator<gAddr::Cancel>(moveset, offsets->movesetBlock + table->groupCancel, table->groupCancelCount))
	{
		FROM_INDEX(groupCancel.requirements_addr, blockOffset + table->requirement, Requirement);
		FROM_INDEX(groupCancel.extradata_addr, blockOffset + table->cancelExtradata, CancelExtradata);
	}

	// Convert reaction ptrs
	for (auto& reaction : StructIterator<gAddr::Reactions>(moveset, offsets->movesetBlock + table->reactions, table->reactionsCount))
	{
		FROM_INDEX(reaction.front_pushback, blockOffset + table->pushback, Pushback);
		FROM_INDEX(reaction.backturned_pushback, blockOffset + table->pushback, Pushback);
		FROM_INDEX(reaction.left_side_pushback, blockOffset + table->pushback, Pushback);
		FROM_INDEX(reaction.right_side_pushback, blockOffset + table->pushback, Pushback);
		FROM_INDEX(reaction.front_counterhit_pushback, blockOffset + table->pushback, Pushback);
		FROM_INDEX(reaction.downed_pushback, blockOffset + table->pushback, Pushback);
		FROM_INDEX(reaction.block_pushback, blockOffset + table->pushback, Pushback);
	}

	// Convert input sequence ptrs
	for (auto& inputSequence : StructIterator<gAddr::InputSequence>(moveset, offsets->movesetBlock + table->inputSequence, table->inputSequenceCount))
	{
		FROM_INDEX(inputSequence.input_addr, blockOffset + table->input, Input);
	}

	// Convert throwCameras ptrs
	for (auto& throwCameras : StructIterator<gAddr::ThrowCamera>(moveset, offsets->movesetBlock + table->throwCameras, table->throwCamerasCount))
	{
		FROM_INDEX(throwCameras.cameradata_addr, blockOffset + table->cameraData, CameraData);
	}

	// Convert hit conditions ptrs
	for (auto& hitCondition : StructIterator<gAddr::HitCondition>(moveset, offsets->movesetBlock + table->hitCondition, table->hitConditionCount))
	{
		FROM_INDEX(hitCondition.requirements_addr, blockOffset + table->requirement, Requirement);
		FROM_INDEX(hitCondition.reactions_addr, blockOffset + table->reactions, Reactions);
	}

	// Convert pushback ptrs
	for (auto& pushback : StructIterator<gAddr::Pushback>(moveset, offsets->movesetBlock + table->pushback, table->pushbackCount))
	{
		FROM_INDEX(pushback.extradata_addr, blockOffset + table->pushbackExtradata, PushbackExtradata);
	}

	// Convert move-start ptrs
	for (auto& moveBeginProp : StructIterator<gAddr::OtherMoveProperty>(moveset, offsets->movesetBlock + table->moveBeginningProp, table->moveBeginningPropCount))
	{
		FROM_INDEX(moveBeginProp.requirements_addr, blockOffset + table->requirement, Requirement);
	}

	// Convert move-end prop ptrs
	for (auto& moveEndProp : StructIterator<gAddr::OtherMoveProperty>(moveset, offsets->movesetBlock + table->moveEndingProp, table->moveEndingPropCount))
	{
		FROM_INDEX(moveEndProp.requirements_addr, blockOffset + table->requirement, Requirement);
	}
}


// Correct the offsets of the starting pointers of the moveset data
// This doesn't affect gameplay and to my knowledge is not actually used
static void CorrectMovesetInfoValues(MovesetInfo* info, gameAddr gameMoveset)
{
	info->character_name_addr += gameMoveset;
	info->character_creator_addr += gameMoveset;
	info->date_addr += gameMoveset;
	info->fulldate_addr += gameMoveset;
}


// Enforce the alias at the time of the extraction to be the original ones
static void EnforceCurrentAliasesAsDefault(Byte* moveset)
{
	MovesetInfo* infos = (MovesetInfo*)moveset;

	for (int i = 0; i < _countof(infos->current_aliases); ++i) {
		infos->orig_aliases[i] = infos->current_aliases[i];
	}
}


// Enforce the original aliases as the current one
static void EnforceDefaultAliasesAsCurrent(Byte* moveset)
{
	MovesetInfo* infos = (MovesetInfo*)moveset;

	for (int i = 0; i < _countof(infos->current_aliases); ++i) {
		infos->current_aliases[i] = infos->orig_aliases[i];
	}
}


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
		if (fileMotas[i] != MOVESET_ADDR_MISSING) {
			fileMotas[i] += gameMoveset + offsets->motaBlock;
		}
		else if (replaceEmpty) {
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
			requirement[i].param_unsigned = requirement[i].param_unsigned == movesetCharacterId ? currentCharacterId : currentCharacterId + 1;
		}
	}
}


void ImporterT7::ImportMovelist(MvlHead* mvlHead, gameAddr game_mlvHead, gameAddr playerAddress)
{
	gameAddr managerAddr = m_game->ReadPtr("movelist_manager_addr");

	int playerId = m_process->readInt32(playerAddress + m_game->GetValue("playerid_offset"));

	if (playerId == 1) {
		managerAddr += sizeof(MvlManager);
	}

	m_process->writeInt64(managerAddr + offsetof(MvlManager, mvlHead), game_mlvHead);
	m_process->writeInt64(managerAddr + offsetof(MvlManager, displayableEntriesCount), mvlHead->displayables_count);
	m_process->writeInt64(managerAddr + offsetof(MvlManager, mvlDisplayableBlock), game_mlvHead + mvlHead->displayables_offset);
}


ImportationErrcode_ ImporterT7::_Import(Byte* moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	progress = 15;
	ImportationErrcode_ errCode = ImportationErrcode_UnsupportedGameVersion;

	// Header of the moveset that will contain our own information about it
	const TKMovesetHeader* header = (TKMovesetHeader*)moveset;

	bool isCompressed = header->isCompressed();
	auto gameId = header->gameId;

	if (settings & ImportSettings_ImportOriginalData) {
		const gameAddr gameOriginalMoveset = m_process->allocateMem(s_moveset);
		if (gameOriginalMoveset == 0) {
			return ImportationErrcode_GameAllocationErr;
		}
		m_process->writeBytes(gameOriginalMoveset, moveset, s_moveset);
		lastLoaded.originalDataAddress = gameOriginalMoveset;
		lastLoaded.originalDataSize = s_moveset;
	}
	else {
		lastLoaded.originalDataAddress = 0;
		lastLoaded.originalDataSize = 0;
	}

	if (isCompressed) {
		uint64_t src_size = s_moveset - header->moveset_data_start;

		moveset = CompressionUtils::RAW::Moveset::Decompress(moveset, src_size, s_moveset);

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

	switch (gameId)
	{
	case GameId_T7:
		errCode = _Import_FromT7(header, moveset, s_moveset, playerAddress, settings, progress);
		break;
	}

	if (isCompressed) {
		delete[] moveset;
	}

	return errCode;
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
	gameAddr playerAddress = m_game->ReadPtr("p1_addr");
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
	uint64_t mvlManagerAddr = m_game->ReadPtr("movelist_manager_addr");
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
	gameAddr playerAddress = m_game->ReadPtr("p1_addr");
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
	gameAddr playerAddress = m_game->ReadPtr("p1_addr");
	if (playerId > 0) {
		playerAddress += playerId * m_game->GetValue("playerstruct_size");
	}
	return playerAddress;
}


gameAddr ImporterT7::GetMovesetAddress(uint8_t playerId)
{
	gameAddr playerAddress = m_game->ReadPtr("p1_addr");
	if (playerAddress > 0) {
		playerAddress += playerId * m_game->GetValue("playerstruct_size");
		return m_process->readInt64(playerAddress + m_game->GetValue("motbin_offset"));
	}
	return 0;
}
