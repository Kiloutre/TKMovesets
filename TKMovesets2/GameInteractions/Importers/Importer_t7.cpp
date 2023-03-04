#include <string>

#include "helpers.hpp"
#include "Importer_t7.hpp"

#include "Structs_t7.h"

// Contains the same structure as StructsT7 but with gameAddr types instead of ptrs types
// Defined here because i don't want any other file to have access to this shortcut
#define gAddr StructsT7_gameAddr

// -- Static helpers -- //

// Reads the moveset header size, the moveset size (post header), allocate the moveset in our own memory and write to it
static char* getMovesetInfos(std::ifstream& file, MovesetHeader* header, uint64_t& size_out)
{
	file.read((char*)header, sizeof(MovesetHeader));
	file.seekg(0, std::ios::end);
	size_out = file.tellg();
	char* movesetData = (char*)malloc(size_out - header->infos.header_size);
	if (movesetData != nullptr) {
		file.seekg( header->infos.header_size + header->offsets.movesetInfoBlock, std::ios::beg);
		file.read(movesetData, size_out);
	}
	file.close();
	return movesetData;
}

// -- Private methods -- //

void ImporterT7::SetCurrentMove(gameAddr playerAddress, gameAddr playerMoveset, size_t moveId)
{
	{
		/// Yes, this needs to be done here, this is only useful when we really want to set the current move
		// Each player actually holds 5 copies of its moveset ptr.
		// One of them is the one to default to when going back to aliases move (>= 32768)
		// One of them is the one of the move that is currently playing, which may come from the opponent's moveset
		gameAddr movesetOffset = playerAddress + m_game->addrFile->GetSingleValue("val_motbin_offset");
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
	m_process->writeInt32(playerAddress + 0x1D4, 99999);
	// Tell the game which move to play NEXT
	m_process->writeInt64(playerAddress + 0xDA0, moveAddr);
	// Also tell the ID of the current move. This isn't required per se, but not doing that would make the current move ID 0, which i don't like.
	m_process->writeInt64(playerAddress + 0x350, moveId);
}

void ImporterT7::ConvertMotaListOffsets(uint64_t motalistsBlock, char* movesetData, gameAddr gameMoveset, gameAddr playerAddress)
{
	MotaList currentMotasList{};
	gameAddr currentMovesetAddr = m_process->readInt64(playerAddress + m_game->addrFile->GetSingleValue("val_motbin_offset"));
	m_process->readBytes(currentMovesetAddr + 0x280, &currentMotasList, sizeof(MotaList));

	MotaList* motaList = (MotaList*)(movesetData + motalistsBlock);

	uint64_t* gameMotaCursor = (uint64_t*)&currentMotasList;
	uint64_t* fileMotaCursor = (uint64_t*)motaList;

	// This is just a list of uint64_t anyway so might as well do this
	for (size_t i = 0; i <= 12; ++i)
	{
		if (fileMotaCursor[i] == 0 || true) {
			// Moveset block was not included in the file: copy the currently used one
			fileMotaCursor[i] = gameMotaCursor[i];
		}
		else
		{
			// Todo: check if this works. I believe we are off.
			fileMotaCursor[i] += gameMoveset;
		}
	}
}

void ImporterT7::ConvertMovesetTableOffsets(const MovesetHeader_offsets& offsets, char* moveset, gameAddr gameMoveset)
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
	table->unknown_0x1f0 += offset;
	table->unknown_0x200 += offset;
	table->move += offset;
	table->voiceclip += offset;
	table->inputSequence += offset;
	table->input += offset;
	table->unknownParryRelated += offset;
	table->cameraData += offset;
	table->throws += offset;
}

void ImporterT7::ConvertMovesetIndexes(char* moveset, gameAddr gameMoveset, const gAddr::MovesetTable* offsets, const MovesetHeader_offsets& blockOffsets)
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
		FROM_INDEX(move->hit_condition_addr, blockOffset + offsets->hitCondition, HitCondition);
		FROM_INDEX(move->voicelip_addr, blockOffset + offsets->voiceclip, Voiceclip);
		FROM_INDEX(move->extra_move_property_addr, blockOffset + offsets->extraMoveProperty, ExtraMoveProperty);
	}

	// Convert projectile ptrs
	i = 0;
	for (gAddr::Projectile* projectile = (gAddr::Projectile*)(moveset + blockOffsets.movesetBlock + offsets->projectile); i < offsets->projectileCount; ++i, ++projectile)
	{
		// One projectile actually has both at 0 for some reason ?
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

	// Convert ??? ptrs
	i = 0;
	for (gAddr::unknown_0x200* unknown = (gAddr::unknown_0x200*)(moveset + blockOffsets.movesetBlock + offsets->unknown_0x200); i < offsets->unknown_0x200_size; ++i, ++unknown)
	{
		FROM_INDEX(unknown->requirements_addr, blockOffset + offsets->requirement, Requirement);
	}
}

// -- Public methods -- //

void ImporterT7::CleanupUnusedMovesets()
{
	gameAddr playerAddress = m_game->ReadPtr("p1_addr");
	uint64_t playerstructSize = m_game->addrFile->GetSingleValue("val_playerstruct_size");
	uint64_t motbinOffset = m_game->addrFile->GetSingleValue("val_motbin_offset");

	std::vector<uint64_t> offsetsToWatch = { 0x218, 0x220, 0x228, 0xBC8, 0xD80, 0x13C0, 0x13E8, 0x13F0, 0x1520, 0x1528, 0x1530, 0x1540};

	for (size_t i = 0; i < m_process->allocatedMemory.size();)
	{
		std::pair<gameAddr, uint64_t> block = m_process->allocatedMemory[i];
		gameAddr movesetAddress = block.first;
		uint64_t movesetEnd = movesetAddress + block.second;
		bool isUsed = false;

		// Call CanImport() because it's a quick way to see if players are loaded. If they're not, we can free memory worry-free.
		if (CanImport()) {
			// Check movesets of both players
			for (size_t playerid = 0; playerid < 2 && !isUsed; ++playerid) {
				gameAddr movesetOffsets = playerAddress + motbinOffset + playerid * playerstructSize;

				// Check a bunch of offsets that are likely to contain moveset
				for (uint64_t offset : offsetsToWatch) {
					gameAddr offsetValue = m_process->readInt64(playerAddress + offset + playerid * playerstructSize);
					if (movesetAddress <= offsetValue && offsetValue < movesetEnd) {
						isUsed = true;
						break;
					}
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

ImportationErrcode ImporterT7::Import(const char* filename, gameAddr playerAddress, bool applyInstantly, float& progress)
{
	progress = 0;
	// Read file data
	std::ifstream file(filename, std::ios::binary);

	if (file.fail()) {
		return ImportationFileReadErr;
	}

	// Variables that will store the moveset size & the moveset itself in our own memory
	uint64_t s_moveset;
	char* moveset;

	// Header of the moveset that will contain our own information about it
	MovesetHeader header;

	// Table that contains offsets and amount of cancels, move, requirements, etc...
	gAddr::MovesetTable* offsets;

	// Moveset allocated IN-GAME. 
	gameAddr gameMoveset;

	// -- File reading & allocations -- //


	// Allocate a copy of the moveset locally. This is NOT in the game's memory
	moveset = getMovesetInfos(file, &header, s_moveset);
	if (moveset == nullptr) {
		return ImportationAllocationErr;
	}
	progress = 20;


	// Allocate our moveset in the game's memory, but we aren't gonna write on that for a while.
	// The idea is to write on our moveset in our own memory (should be faster), then write it all at once on gameMoveset with a single m_process->writeBytes()
	gameMoveset = m_process->allocateMem(s_moveset);
	if (gameMoveset == 0) {
		free(moveset);
		return ImportationGameAllocationErr;
	}
	progress = 40;

	// -- Conversions -- //


	// Get the table address
	offsets = (gAddr::MovesetTable*)(moveset + header.offsets.tableBlock);


	//Convert move offets into ptrs
	ConvertMovesetIndexes(moveset, gameMoveset, offsets, header.offsets);
	progress = 70;

	// Turn our table offsets into ptrs. Do this only at the end because we actually need those offsets above
	ConvertMovesetTableOffsets(header.offsets, moveset, gameMoveset);
	progress = 80;

	// Turn our mota offsets into mota ptrs, or copy the currently loaded character's mota for each we didn't provide
	ConvertMotaListOffsets(header.offsets.motalistsBlock, moveset, gameMoveset, playerAddress);
	progress = 90;


	// -- Allocation &Conversion finished -- //

	// Finally write our moveset to the game's memory
	m_process->writeBytes(gameMoveset, moveset, s_moveset);
	progress = 99;

	// Then write our moveset address to the current player

	m_process->writeInt64(playerAddress + m_game->addrFile->GetSingleValue("val_motbin_offset"), gameMoveset);
	progress = 100;

	if (applyInstantly) {
		SetCurrentMove(playerAddress, gameMoveset, 32769);
	}

	// -- Cleanup -- //

	free(moveset);
	return ImportationSuccessful;
}

bool ImporterT7::CanImport()
{
	gameAddr playerAddress = m_game->ReadPtr("p1_addr");
	// We'll just read through a bunch of values that wouldn't be valid if a moveset wasn't loaded
	// readInt64() may return -1 if the read fails so we have to check for this value as well.

	if (playerAddress == 0 || playerAddress == -1) {
		return false;
	}

	gameAddr currentMove = m_process->readInt64(playerAddress + 0x220);
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
