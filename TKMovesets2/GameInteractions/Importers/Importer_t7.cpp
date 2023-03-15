#include <string>

#include "helpers.hpp"
#include "Importer_t7.hpp"

#include "Structs_t7.h"

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
		gameAddr movesetOffset = playerAddress + m_game->addrFile->GetSingleValue("val:t7_motbin_offset");
		for (size_t i = 1; i < 5; ++i) {
			m_process->writeInt64(movesetOffset + i * 8, playerMoveset);
		}
	}

	if (moveId >= 0x8000) {
		// If is alias, convert it to its regular move id thanks to the alias list (uint16_t each) starting at 0x28
		moveId = m_process->readInt16(playerMoveset + 0x28 + (0x2 * (moveId - 0x8000)));
	}

	gameAddr moveAddr = m_process->readInt64(playerMoveset + 0x210) + moveId * sizeof(Move);

	// Write a big number to the frame timer to force the current move end
	m_process->writeInt32(playerAddress + m_game->addrFile->GetSingleValue("val:t7_currmove_timer"), 99999);
	// Tell the game which move to play NEXT
	m_process->writeInt64(playerAddress + m_game->addrFile->GetSingleValue("val:t7_nextmove_addr"), moveAddr);
	// Also tell the ID of the current move. This isn't required per se, but not doing that would make the current move ID 0, which i don't like.
	m_process->writeInt64(playerAddress + m_game->addrFile->GetSingleValue("val:t7_currmove_id"), moveId);
}

void ImporterT7::WriteCameraMotasToPlayer(gameAddr movesetAddr, gameAddr playerAddress)
{
	const uint64_t staticCameraOffset = m_game->addrFile->GetSingleValue("val:t7_camera_mota_offset");
	gameAddr cameraMota1 = m_process->readInt64(movesetAddr + 0x2C0);
	gameAddr cameraMota2 = m_process->readInt64(movesetAddr + 0x2C8);

	m_process->writeInt64(playerAddress + staticCameraOffset, cameraMota1);
	m_process->writeInt64(playerAddress + staticCameraOffset + 0x8, cameraMota2);
}

void ImporterT7::ConvertMotaListOffsets(const TKMovesetHeader_offsets& offsets, Byte* moveset, gameAddr gameMoveset, gameAddr playerAddress)
{
	MotaList currentMotasList{};
	gameAddr currentMovesetAddr = m_process->readInt64(playerAddress + m_game->addrFile->GetSingleValue("val:t7_motbin_offset"));
	m_process->readBytes(currentMovesetAddr + 0x280, &currentMotasList, sizeof(MotaList));

	MotaList* motaList = (MotaList*)(moveset + offsets.motalistsBlock);

	uint64_t* gameMotaCursor = (uint64_t*)&currentMotasList;
	uint64_t* fileMotaCursor = (uint64_t*)motaList;

	// This is just a list of uint64_t anyway so might as well do this
	for (size_t i = 0; i <= 12; ++i)
	{
		if (fileMotaCursor[i] == MOVESET_ADDR_MISSING) {
			// Moveset block was not included in the file: copy the currently used one
			fileMotaCursor[i] = gameMotaCursor[i];
		}
		else {
			fileMotaCursor[i] += gameMoveset + offsets.motaBlock;
		}
	}
}

void ImporterT7::ConvertMovesetTableOffsets(const TKMovesetHeader_offsets& offsets, Byte* moveset, gameAddr gameMoveset)
{
	gAddr::MovesetTable* table = (gAddr::MovesetTable*)(moveset + offsets.tableBlock);
	gameAddr offset = gameMoveset + offsets.movesetBlock;

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
	table->throws += offset;
}

void ImporterT7::ApplyCharacterIDFixes(Byte* moveset, gameAddr playerAddress, const gAddr::MovesetTable* offsets, const TKMovesetHeader& header)
{
	// In movesets, some moves (for some reason) can be transitionned into only on specific character IDs
	// I am taking about mundane moves such as EWHF not working where WHF does
	// The why is hard to understand and might possibly be linked to Mokujin/Combot, but anyway, this fixes things
	uint16_t movesetCharacterId = header.infos.characterId;
	uint16_t currentCharacterId = m_process->readInt16(playerAddress + m_game->addrFile->GetSingleValue("val:t7_chara_id_offset"));

	Requirement* requirement = (Requirement*)(moveset + header.offsets.movesetBlock + offsets->requirement);

	for (size_t i = 0; i < offsets->requirementCount; ++i)
	{
		// 217 = Is current char specific ID
		// When the requirement ask "am i X character ID", X = extracted character ID
		// i will change that X character ID to be the one of the current character, to make it always true.
		// When the requirement ask for any other character ID, i will supply a character ID that ISN'T the current one, to make it always false.

		if (requirement[i].condition == 217) {
			requirement[i].param = requirement[i].param == movesetCharacterId ? currentCharacterId : currentCharacterId + 1;
		}
	}
}

void ImporterT7::ConvertMovesetIndexes(Byte* moveset, gameAddr gameMoveset, const gAddr::MovesetTable* offsets, const TKMovesetHeader_offsets& blockOffsets)
{
	size_t i;
	gameAddr blockOffset = gameMoveset + blockOffsets.movesetBlock;

	i = 0;
	for (gAddr::Move* move = (gAddr::Move*)(moveset + blockOffsets.movesetBlock + offsets->move); i < offsets->moveCount; ++i, ++move)
	{
		move->name_addr += gameMoveset + blockOffsets.nameBlock;
		move->anim_name_addr += gameMoveset + blockOffsets.nameBlock;
		move->anim_addr += gameMoveset + blockOffsets.animationBlock;

		FROM_INDEX(move->cancel_addr, blockOffset + offsets->cancel, Cancel);
		FROM_INDEX(move->_0x28_cancel_addr, blockOffset + offsets->cancel, Cancel);
		FROM_INDEX(move->_0x38_cancel_addr, blockOffset + offsets->cancel, Cancel);
		FROM_INDEX(move->_0x48_cancel_addr, blockOffset + offsets->cancel, Cancel);
		FROM_INDEX(move->hit_condition_addr, blockOffset + offsets->hitCondition, HitCondition);
		FROM_INDEX(move->voicelip_addr, blockOffset + offsets->voiceclip, Voiceclip);
		FROM_INDEX(move->extra_move_property_addr, blockOffset + offsets->extraMoveProperty, ExtraMoveProperty);
		FROM_INDEX(move->move_start_extraprop_addr, blockOffset + offsets->moveBeginningProp, OtherMoveProperty);
		FROM_INDEX(move->move_end_extraprop_addr, blockOffset + offsets->moveEndingProp, OtherMoveProperty);
	}

	// Convert projectile ptrs
	i = 0;
	for (gAddr::Projectile* projectile = (gAddr::Projectile*)(moveset + blockOffsets.movesetBlock + offsets->projectile); i < offsets->projectileCount; ++i, ++projectile)
	{
		// One projectile actually has both at 0 for some reason ? todo : check
		FROM_INDEX(projectile->cancel_addr, blockOffset + offsets->cancel, Cancel);
		FROM_INDEX(projectile->hit_condition_addr, blockOffset + offsets->hitCondition, HitCondition);
	}

	// Convert cancel ptrs
	i = 0;
	for (gAddr::Cancel* cancel = (gAddr::Cancel*)(moveset + blockOffsets.movesetBlock + offsets->cancel); i < offsets->cancelCount; ++i, ++cancel)
	{
		FROM_INDEX(cancel->requirement_addr, blockOffset + offsets->requirement, Requirement);
		FROM_INDEX(cancel->extradata_addr, blockOffset + offsets->cancelExtradata, CancelExtradata);
	}
	i = 0;

	for (gAddr::Cancel* groupCancel = (gAddr::Cancel*)(moveset + blockOffsets.movesetBlock + offsets->groupCancel); i < offsets->groupCancelCount; ++i, ++groupCancel)
	{
		FROM_INDEX(groupCancel->requirement_addr, blockOffset + offsets->requirement, Requirement);
		FROM_INDEX(groupCancel->extradata_addr, blockOffset + offsets->cancelExtradata, CancelExtradata);
	}

	// Convert reaction ptrs
	i = 0;
	for (gAddr::Reactions* reaction = (gAddr::Reactions*)(moveset + blockOffsets.movesetBlock + offsets->reactions); i < offsets->reactionsCount; ++i, ++reaction)
	{
		FROM_INDEX(reaction->front_pushback, blockOffset + offsets->pushback, Pushback);
		FROM_INDEX(reaction->backturned_pushback, blockOffset + offsets->pushback, Pushback);
		FROM_INDEX(reaction->left_side_pushback, blockOffset + offsets->pushback, Pushback);
		FROM_INDEX(reaction->right_side_pushback, blockOffset + offsets->pushback, Pushback);
		FROM_INDEX(reaction->front_counterhit_pushback, blockOffset + offsets->pushback, Pushback);
		FROM_INDEX(reaction->downed_pushback, blockOffset + offsets->pushback, Pushback);
		FROM_INDEX(reaction->block_pushback, blockOffset + offsets->pushback, Pushback);
	}

	// Convert input sequence ptrs
	i = 0;
	for (gAddr::InputSequence* inputSequence = (gAddr::InputSequence*)(moveset + blockOffsets.movesetBlock + offsets->inputSequence); i < offsets->inputSequenceCount; ++i, ++inputSequence)
	{
		FROM_INDEX(inputSequence->input_addr, blockOffset + offsets->input, Input);
	}

	// Convert throws ptrs
	i = 0;
	for (gAddr::ThrowData* throws = (gAddr::ThrowData*)(moveset + blockOffsets.movesetBlock + offsets->throws); i < offsets->throwsCount; ++i, ++throws)
	{
		FROM_INDEX(throws->cameradata_addr, blockOffset + offsets->cameraData, CameraData);
	}

	// Convert hit conditions ptrs
	i = 0;
	for (gAddr::HitCondition* hitCondition = (gAddr::HitCondition*)(moveset + blockOffsets.movesetBlock + offsets->hitCondition); i < offsets->hitConditionCount; ++i, ++hitCondition)
	{
		FROM_INDEX(hitCondition->requirement_addr, blockOffset + offsets->requirement, Requirement);
		FROM_INDEX(hitCondition->reactions_addr, blockOffset + offsets->reactions, Reactions);
	}

	// Convert pushback ptrs
	i = 0;
	for (gAddr::Pushback* pushback = (gAddr::Pushback*)(moveset + blockOffsets.movesetBlock + offsets->pushback); i < offsets->pushbackCount; ++i, ++pushback)
	{
		FROM_INDEX(pushback->extradata_addr, blockOffset + offsets->pushbackExtradata, PushbackExtradata);
	}

	// Convert move-start ptrs
	i = 0;
	for (gAddr::OtherMoveProperty* moveBeginProp = (gAddr::OtherMoveProperty*)(moveset + blockOffsets.movesetBlock + offsets->moveBeginningProp); i < offsets->moveBeginningPropCount; ++i, ++moveBeginProp)
	{
		FROM_INDEX(moveBeginProp->requirements_addr, blockOffset + offsets->requirement, Requirement);
	}

	// Convert move-end prop ptrs
	i = 0;
	for (gAddr::OtherMoveProperty* moveEndProp = (gAddr::OtherMoveProperty*)(moveset + blockOffsets.movesetBlock + offsets->moveEndingProp); i < offsets->moveEndingPropCount; ++i, ++moveEndProp)
	{
		FROM_INDEX(moveEndProp->requirements_addr, blockOffset + offsets->requirement, Requirement);
	}
}

// -- Public methods -- //

void ImporterT7::CleanupUnusedMovesets()
{
	gameAddr playerAddress = m_game->ReadPtr("t7_p1_addr");
	uint64_t playerstructSize = m_game->addrFile->GetSingleValue("val:t7_playerstruct_size");
	uint64_t motbinOffset = m_game->addrFile->GetSingleValue("val:t7_motbin_offset");

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

	for (size_t i = 0; i + 1 < m_process->allocatedMemory.size();)
	{
		std::pair<gameAddr, uint64_t> block = m_process->allocatedMemory[i];
		gameAddr movesetAddress = block.first;
		uint64_t movesetEnd = movesetAddress + block.second;
		bool isUsed = false;

		// Call CanImport() because it's a quick way to see if players are loaded. If they're not, we can free memory worry-free.
		if (CanImport()) {
			// Check movesets of both players
			for (size_t playerid = 0; playerid < 2 && !isUsed; ++playerid) {
				gameAddr currentPlayerAddress = playerAddress + playerid * playerstructSize;

				// Maybe overkill to check every offset, but drastically reduces the chance of a crash, and isn't that slow
				gameAddr pStart = currentPlayerAddress;
				gameAddr pEnd = pStart + playerstructSize;
				while (pStart < pEnd)
				{
					uint64_t offsetValue = m_process->readInt64(pStart);
					if (movesetAddress <= offsetValue && offsetValue < movesetEnd) {
						isUsed = true;
					}
					pStart += 8;
				}

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

static void CorrectMovesetInfoValues(MovesetInfo* info, gameAddr gameMoveset)
{
	// Todo: write the actual, proper values for these
	info->character_creator_addr = (char*)(gameMoveset + 0x2E8);
	info->date_addr = (char*)(gameMoveset + 0x2E8);
	info->fulldate_addr = (char*)(gameMoveset + 0x2E8);
	info->character_creator_addr = (char*)(gameMoveset + 0x2E8);
}

ImportationErrcode_ ImporterT7::Import(const Byte* orig_moveset, uint64_t s_moveset, gameAddr playerAddress, bool applyInstantly, uint8_t& progress)
{
	// Will contain our moveset copy. We do not modify orig_moveset.
	Byte* moveset;

	// Header of the moveset that will contain our own information about it
	TKMovesetHeader header;

	// Table that contains offsets and amount of cancels, move, requirements, etc...
	gAddr::MovesetTable* table;

	// -- File reading & allocations -- //

	// Read important header information (our own header)
	memcpy_s(&header, sizeof(TKMovesetHeader), orig_moveset, sizeof(TKMovesetHeader));
	progress = 20;

	{
		// Make a copy of the moveset that we can modify freely and quickly before importing
		uint64_t movesetStartOffset = header.infos.header_size + header.offsets.movesetInfoBlock;
		s_moveset -= movesetStartOffset;
		moveset = (Byte*)malloc(s_moveset);
		if (moveset == nullptr) {
			return ImportationErrcode_AllocationErr;
		}
		memcpy(moveset, orig_moveset + movesetStartOffset, s_moveset);
	}

	// Allocate our moveset in the game's memory, but we aren't gonna write on that for a while.
	// The idea is to write on our moveset in our own memory (should be faster), then write it all at once on gameMoveset with a single m_process->writeBytes()
	const gameAddr gameMoveset = m_process->allocateMem(s_moveset);
	if (gameMoveset == 0) {
		free(moveset);
		return ImportationErrcode_GameAllocationErr;
	}
	progress = 40;

	// -- Conversions -- //


	// Get the table address
	table = (gAddr::MovesetTable*)(moveset + header.offsets.tableBlock);

	CorrectMovesetInfoValues((MovesetInfo*)moveset, gameMoveset);

	//Convert move offets into ptrs
	ConvertMovesetIndexes(moveset, gameMoveset, table, header.offsets);
	progress = 70;

	// Fix moves that use characterID conditions to work
	ApplyCharacterIDFixes(moveset, playerAddress, table, header);
	progress = 75;

	// Turn our table offsets into ptrs. Do this only at the end because we actually need those offsets above
	ConvertMovesetTableOffsets(header.offsets, moveset, gameMoveset);
	progress = 80;

	// Turn our mota offsets into mota ptrs, or copy the currently loaded character's mota for each we didn't provide
	ConvertMotaListOffsets(header.offsets, moveset, gameMoveset, playerAddress);
	progress = 90;

	// -- Allocation & Conversion finished -- //

	// Finally write our moveset to the game's memory
	m_process->writeBytes(gameMoveset, moveset, s_moveset);
	progress = 99;

	// Then write our moveset address to the current player
	m_process->writeInt64(playerAddress + m_game->addrFile->GetSingleValue("val:t7_motbin_offset"), gameMoveset);
	progress = 100;

	// Also write camera mota offsts to the player structure if those motas have been exported
	WriteCameraMotasToPlayer(gameMoveset, playerAddress);

	if (applyInstantly) {
		ForcePlayerMove(playerAddress, gameMoveset, 32769);
	}

	// -- Cleanup -- //
	// Destroy our local copy
	free(moveset);

	lastLoadedMoveset = gameMoveset;
	return ImportationErrcode_Successful;
}

bool ImporterT7::CanImport()
{
	// todo: this is invalid, because when we import our own moveset and leave back to main menu, it will return true
	// yes we can import in that case but it will serve zero purpose
	gameAddr playerAddress = m_game->ReadPtr("t7_p1_addr");
	// We'll just read through a bunch of values that wouldn't be valid if a moveset wasn't loaded
	// readInt64() may return -1 if the read fails so we have to check for this value as well.

	if (playerAddress == 0 || playerAddress == -1) {
		return false;
	}

	gameAddr currentMove = m_process->readInt64(playerAddress + m_game->addrFile->GetSingleValue("val:t7_currmove"));
	if (currentMove == 0 || currentMove == -1) {
		return false;
	}

	gameAddr animAddr = m_process->readInt64(currentMove + 0x10);
	if (animAddr == 0 || animAddr == -1) {
		return false;
	}

	uint8_t animType = m_process->readInt8(animAddr);
	return animType == 0x64 || animType == 0xC8;
}

gameAddr ImporterT7::GetCharacterAddress(uint8_t playerId)
{
	gameAddr playerAddress = m_game->ReadPtr("t7_p1_addr");
	if (playerId > 0) {
		playerAddress += playerId * m_game->addrFile->GetSingleValue("val:t7_playerstruct_size");
	}
	return playerAddress;
}

gameAddr ImporterT7::GetMovesetAddress(uint8_t playerId)
{
	gameAddr playerAddress = m_game->ReadPtr("t7_p1_addr");
	if (playerAddress > 0) {
		playerAddress += playerId * m_game->addrFile->GetSingleValue("val:t7_playerstruct_size");
		return m_process->readInt64(playerAddress + m_game->addrFile->GetSingleValue("val:t7_motbin_offset"));
	}
	return 0;
}
