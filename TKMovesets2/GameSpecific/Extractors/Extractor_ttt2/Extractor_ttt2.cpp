#include <set>
#include <string>
#include <cctype>
#include <stddef.h>
#include <iterator>
#include <vector>
#include <algorithm>
#include <filesystem>

#include "helpers.hpp"
#include "Compression.hpp"
#include "Extractor_ttt2.hpp"
#include "Animations.hpp"

using namespace StructsTTT2;

// -- Static helpers -- //

// Converts absolute ptr into indexes before saving to file
static void convertMovesetDataToLittleEndian(Byte* movesetBlock, const MovesetTable* offsets)
{
	for (auto& pushbackExtra : StructIterator<PushbackExtradata>(movesetBlock, offsets->pushbackExtradata, offsets->pushbackExtradataCount))
	{
		ByteswapHelpers::SWAP_INT16(&pushbackExtra.horizontal_offset);
	}

	for (auto& pushback : StructIterator<Pushback>(movesetBlock, offsets->pushback, offsets->pushbackCount))
	{
		ByteswapHelpers::SWAP_INT16(&pushback.duration);
		ByteswapHelpers::SWAP_INT16(&pushback.displacement);
		ByteswapHelpers::SWAP_INT32(&pushback.num_of_loops);
		ByteswapHelpers::SWAP_INT32(&pushback.extradata_addr);
	}

	for (auto& req : StructIterator<Requirement>(movesetBlock, offsets->requirement, offsets->requirementCount))
	{
		ByteswapHelpers::SWAP_INT32(&req.condition);
		ByteswapHelpers::SWAP_INT32(&req.param_unsigned);
	}

	for (auto& cancelExtra : StructIterator<CancelExtradata>(movesetBlock, offsets->cancelExtradata, offsets->cancelExtradataCount))
	{
		ByteswapHelpers::SWAP_INT32(&cancelExtra.value);
	}

	for (auto& cancel : StructIterator<Cancel>(movesetBlock, offsets->cancel, offsets->cancelCount))
	{
		ByteswapHelpers::SWAP_INT32(&cancel.direction);
		ByteswapHelpers::SWAP_INT32(&cancel.button);

		ByteswapHelpers::SWAP_INT32(&cancel.requirements_addr);
		ByteswapHelpers::SWAP_INT32(&cancel.extradata_addr);
		ByteswapHelpers::SWAP_INT32(&cancel.detection_start);
		ByteswapHelpers::SWAP_INT32(&cancel.detection_end);
		ByteswapHelpers::SWAP_INT32(&cancel.starting_frame);
		ByteswapHelpers::SWAP_INT16(&cancel.move_id);
		ByteswapHelpers::SWAP_INT16(&cancel.cancel_option);
	}

	for (auto& groupCancel : StructIterator<Cancel>(movesetBlock, offsets->groupCancel, offsets->groupCancelCount))
	{
		ByteswapHelpers::SWAP_INT32(&groupCancel.direction);
		ByteswapHelpers::SWAP_INT32(&groupCancel.button);

		ByteswapHelpers::SWAP_INT32(&groupCancel.requirements_addr);
		ByteswapHelpers::SWAP_INT32(&groupCancel.extradata_addr);
		ByteswapHelpers::SWAP_INT32(&groupCancel.detection_start);
		ByteswapHelpers::SWAP_INT32(&groupCancel.detection_end);
		ByteswapHelpers::SWAP_INT32(&groupCancel.starting_frame);
		ByteswapHelpers::SWAP_INT16(&groupCancel.move_id);
		ByteswapHelpers::SWAP_INT16(&groupCancel.cancel_option);
	}

	for (auto& reaction : StructIterator<Reactions>(movesetBlock, offsets->reactions, offsets->reactionsCount))
	{
		ByteswapHelpers::SWAP_INT32(&reaction.front_pushback);
		ByteswapHelpers::SWAP_INT32(&reaction.backturned_pushback);
		ByteswapHelpers::SWAP_INT32(&reaction.left_side_pushback);
		ByteswapHelpers::SWAP_INT32(&reaction.right_side_pushback);
		ByteswapHelpers::SWAP_INT32(&reaction.front_counterhit_pushback);
		ByteswapHelpers::SWAP_INT32(&reaction.downed_pushback);
		ByteswapHelpers::SWAP_INT32(&reaction.block_pushback);

		ByteswapHelpers::SWAP_INT16(&reaction.front_direction);
		ByteswapHelpers::SWAP_INT16(&reaction.back_direction);
		ByteswapHelpers::SWAP_INT16(&reaction.left_side_direction);
		ByteswapHelpers::SWAP_INT16(&reaction.right_side_direction);
		ByteswapHelpers::SWAP_INT16(&reaction.front_counterhit_direction);
		ByteswapHelpers::SWAP_INT16(&reaction.downed_direction);

		ByteswapHelpers::SWAP_INT32(&reaction._0x28_int);
		ByteswapHelpers::SWAP_INT32(&reaction._0x2C_int);

		ByteswapHelpers::SWAP_INT16(&reaction.vertical_pushback);
		ByteswapHelpers::SWAP_INT16(&reaction.standing_moveid);
		ByteswapHelpers::SWAP_INT16(&reaction.default_moveid);
		ByteswapHelpers::SWAP_INT16(&reaction.crouch_moveid);
		ByteswapHelpers::SWAP_INT16(&reaction.counterhit_moveid);
		ByteswapHelpers::SWAP_INT16(&reaction.crouch_counterhit_moveid);
		ByteswapHelpers::SWAP_INT16(&reaction.left_side_moveid);
		ByteswapHelpers::SWAP_INT16(&reaction.crouch_left_side_moveid);
		ByteswapHelpers::SWAP_INT16(&reaction.right_side_moveid);
		ByteswapHelpers::SWAP_INT16(&reaction.crouch_right_side_moveid);
		ByteswapHelpers::SWAP_INT16(&reaction.backturned_moveid);
		ByteswapHelpers::SWAP_INT16(&reaction.crouch_backturned_moveid);
		ByteswapHelpers::SWAP_INT16(&reaction.block_moveid);
		ByteswapHelpers::SWAP_INT16(&reaction.crouch_block_moveid);
		ByteswapHelpers::SWAP_INT16(&reaction.wallslump_moveid);
		ByteswapHelpers::SWAP_INT16(&reaction.downed_moveid);
	}

	for (auto& hitCondition : StructIterator<HitCondition>(movesetBlock, offsets->hitCondition, offsets->hitConditionCount))
	{
		ByteswapHelpers::SWAP_INT32(&hitCondition.requirements_addr);
		ByteswapHelpers::SWAP_INT32(&hitCondition.damage);
		ByteswapHelpers::SWAP_INT32(&hitCondition.reactions_addr);
	}

	for (auto& voiceclip : StructIterator<Voiceclip>(movesetBlock, offsets->voiceclip, offsets->voiceclipCount))
	{
		ByteswapHelpers::SWAP_INT32(&voiceclip.id);
	}

	for (auto& extraProp : StructIterator<ExtraMoveProperty>(movesetBlock, offsets->extraMoveProperty, offsets->extraMovePropertyCount))
	{
		ByteswapHelpers::SWAP_INT32(&extraProp.starting_frame);
		ByteswapHelpers::SWAP_INT32(&extraProp.id);
		ByteswapHelpers::SWAP_INT32(&extraProp.value_unsigned);
	}

	for (auto& input : StructIterator<Input>(movesetBlock, offsets->input, offsets->inputCount))
	{
		ByteswapHelpers::SWAP_INT32(&input.direction);
		ByteswapHelpers::SWAP_INT32(&input.button);
	}

	for (auto& inputSequence : StructIterator<InputSequence>(movesetBlock, offsets->inputSequence, offsets->inputSequenceCount))
	{
		// 0x0: int8: not swapping
		// 0x1: int8: not swapping
		ByteswapHelpers::SWAP_INT16(&inputSequence.input_amount);
		ByteswapHelpers::SWAP_INT32(&inputSequence.input_addr);
	}

	for (auto& projectile : StructIterator<Projectile>(movesetBlock, offsets->projectile, offsets->projectileCount))
	{
		// As of this time, don't do any conversion for projectiles
		// Its format is different from the T7 format
		// (The data is still there, in big endian, ready to be parsed by any importer)
	}

	for (auto& cameraData : StructIterator<CameraData>(movesetBlock, offsets->cameraData, offsets->cameraDataCount))
	{
		ByteswapHelpers::SWAP_INT32(&cameraData._0x0_int);
		ByteswapHelpers::SWAP_INT16(&cameraData._0x4_short);
		ByteswapHelpers::SWAP_INT16(&cameraData.left_side_camera_data);
		ByteswapHelpers::SWAP_INT16(&cameraData.right_side_camera_data);
		ByteswapHelpers::SWAP_INT16(&cameraData._0xA_short);
	}

	for (auto& throwCameras : StructIterator<ThrowCamera>(movesetBlock, offsets->throwCameras, offsets->throwCamerasCount))
	{
		ByteswapHelpers::SWAP_INT32(&throwCameras._0x0_uint);
		ByteswapHelpers::SWAP_INT32(&throwCameras.cameradata_addr);
	}


	for (auto& parryRelated : StructIterator<UnknownParryRelated>(movesetBlock, offsets->unknownParryRelated, offsets->unknownParryRelatedCount))
	{
		ByteswapHelpers::SWAP_INT32(&parryRelated.value);
	}

	for (auto& moveBeginningProp : StructIterator<OtherMoveProperty>(movesetBlock, offsets->moveBeginningProp, offsets->moveBeginningPropCount))
	{
		ByteswapHelpers::SWAP_INT32(&moveBeginningProp.requirements_addr);
		ByteswapHelpers::SWAP_INT32(&moveBeginningProp.extraprop);
		ByteswapHelpers::SWAP_INT32(&moveBeginningProp.value);
	}

	for (auto& moveEndingProp : StructIterator<OtherMoveProperty>(movesetBlock, offsets->moveEndingProp, offsets->moveEndingPropCount))
	{
		ByteswapHelpers::SWAP_INT32(&moveEndingProp.requirements_addr);
		ByteswapHelpers::SWAP_INT32(&moveEndingProp.extraprop);
		ByteswapHelpers::SWAP_INT32(&moveEndingProp.value);
	}


	for (auto& move : StructIterator<Move>(movesetBlock, offsets->move, offsets->moveCount))
	{
		ByteswapHelpers::SWAP_INT32(&move.name_addr);
		ByteswapHelpers::SWAP_INT32(&move.anim_name_addr);
		ByteswapHelpers::SWAP_INT32(&move.anim_addr);
		ByteswapHelpers::SWAP_INT32(&move.vuln);
		ByteswapHelpers::SWAP_INT32(&move.hitlevel);
		ByteswapHelpers::SWAP_INT32(&move.cancel_addr);

		//... more fields omitted here
		// todo

		ByteswapHelpers::SWAP_INT16(&move.transition);

		ByteswapHelpers::SWAP_INT16(&move._0x56_short);

		// Swaps _val1 and 8val2 as an int because that is how they are stored
		ByteswapHelpers::SWAP_INT32(&move.moveId_val2);
		ByteswapHelpers::SWAP_INT32(&move.hit_condition_addr);
		ByteswapHelpers::SWAP_INT32(&move.anim_len);
		ByteswapHelpers::SWAP_INT32(&move.airborne_start);
		ByteswapHelpers::SWAP_INT32(&move.airborne_end);
		ByteswapHelpers::SWAP_INT32(&move.ground_fall);
		ByteswapHelpers::SWAP_INT32(&move.voicelip_addr);
		ByteswapHelpers::SWAP_INT32(&move.extra_move_property_addr);
		ByteswapHelpers::SWAP_INT32(&move.move_start_extraprop_addr);
		ByteswapHelpers::SWAP_INT32(&move.move_end_extraprop_addr);
		ByteswapHelpers::SWAP_INT32(&move._0x98_int);
		ByteswapHelpers::SWAP_INT32(&move.hitbox_location);
		ByteswapHelpers::SWAP_INT32(&move.first_active_frame);
		ByteswapHelpers::SWAP_INT32(&move.last_active_frame);
		ByteswapHelpers::SWAP_INT16(&move._0x6c_short);
		ByteswapHelpers::SWAP_INT16(&move.distance);
	}
}

// Converts absolute ptr into indexes before saving to file
static void convertMovesetPointersToIndexes(Byte* movesetBlock, const MovesetTable& table, const MovesetTable* offsets, gameAddr nameStart, std::map<gameAddr, uint64_t>& animOffsetMap)
{
	// Convert move ptrs
	for (auto& move : StructIterator<Move>(movesetBlock, offsets->move, table.moveCount))
	{
		move.name_addr -= (gameAddr32)nameStart;
		move.anim_name_addr -= (gameAddr32)nameStart;
		move.anim_addr = (gameAddr32)animOffsetMap[move.anim_addr];
		TO_INDEX(move.cancel_addr, table.cancel, Cancel);
		TO_INDEX(move._0x28_cancel_addr, table.cancel, Cancel);
		TO_INDEX(move._0x38_cancel_addr, table.cancel, Cancel);
		TO_INDEX(move._0x48_cancel_addr, table.cancel, Cancel);
		TO_INDEX(move.hit_condition_addr, table.hitCondition, HitCondition);
		TO_INDEX(move.voicelip_addr, table.voiceclip, Voiceclip);
		TO_INDEX(move.extra_move_property_addr, table.extraMoveProperty, ExtraMoveProperty);
		TO_INDEX(move.move_start_extraprop_addr, table.moveBeginningProp, OtherMoveProperty);
		TO_INDEX(move.move_end_extraprop_addr, table.moveEndingProp, OtherMoveProperty);
	}

	// Convert cancel ptrs
	for (auto& cancel : StructIterator<Cancel>(movesetBlock, offsets->cancel, table.cancelCount))
	{
		TO_INDEX(cancel.requirements_addr, table.requirement, Requirement);
		TO_INDEX(cancel.extradata_addr, table.cancelExtradata, CancelExtradata);
	}

	// Convert groupCancel cancel ptrs
	for (auto& groupCancel : StructIterator<Cancel>(movesetBlock, offsets->groupCancel, table.groupCancelCount))
	{
		TO_INDEX(groupCancel.requirements_addr, table.requirement, Requirement);
		TO_INDEX(groupCancel.extradata_addr, table.cancelExtradata, CancelExtradata);
	}

	// Convert reaction ptrs
	for (auto& reaction : StructIterator<Reactions>(movesetBlock, offsets->reactions, table.reactionsCount))
	{
		TO_INDEX(reaction.front_pushback, table.pushback, Pushback);
		TO_INDEX(reaction.backturned_pushback, table.pushback, Pushback);
		TO_INDEX(reaction.left_side_pushback, table.pushback, Pushback);
		TO_INDEX(reaction.right_side_pushback, table.pushback, Pushback);
		TO_INDEX(reaction.front_counterhit_pushback, table.pushback, Pushback);
		TO_INDEX(reaction.downed_pushback, table.pushback, Pushback);
		TO_INDEX(reaction.block_pushback, table.pushback, Pushback);
	}

	// Convert input sequence ptrs
	for (auto& inputSequence : StructIterator<InputSequence>(movesetBlock, offsets->inputSequence, table.inputSequenceCount))
	{
		TO_INDEX(inputSequence.input_addr, table.input, Input);
	}

	// Convert throwCameras ptrs
	for (auto& throwCameras : StructIterator<ThrowCamera>(movesetBlock, offsets->throwCameras, table.throwCamerasCount))
	{
		TO_INDEX(throwCameras.cameradata_addr, table.cameraData, CameraData);
	}

	// Convert hit conditions ptrs
	for (auto& hitCondition : StructIterator<HitCondition>(movesetBlock, offsets->hitCondition, table.hitConditionCount))
	{
		TO_INDEX(hitCondition.requirements_addr, table.requirement, Requirement);
		TO_INDEX(hitCondition.reactions_addr, table.reactions, Reactions);
	}

	// Convert pushback ptrs
	for (auto& pushback : StructIterator<Pushback>(movesetBlock, offsets->pushback, table.pushbackCount))
	{
		TO_INDEX(pushback.extradata_addr, table.pushbackExtradata, PushbackExtradata);
	}

	// Convert move start prop ptrs
	for (auto& moveBeginningProp : StructIterator<OtherMoveProperty>(movesetBlock, offsets->moveBeginningProp, table.moveBeginningPropCount))
	{
		TO_INDEX(moveBeginningProp.requirements_addr, table.requirement, Requirement);
	}

	// Convert move end prop ptrs
	for (auto& moveEndingProp : StructIterator<OtherMoveProperty>(movesetBlock, offsets->moveEndingProp, table.moveEndingPropCount))
	{
		TO_INDEX(moveEndingProp.requirements_addr, table.requirement, Requirement);
	}
}

// -- Private methods - //

void ExtractorTTT2::CopyMovesetInfoBlock(gameAddr movesetAddr, MovesetInfo* movesetHeader)
{
	m_game->ReadBytes(movesetAddr, movesetHeader, offsetof(MovesetInfo, table));

	// Byteswap data that needs to be swapped
	ByteswapHelpers::SWAP_INT32(&movesetHeader->character_name_addr);
	ByteswapHelpers::SWAP_INT32(&movesetHeader->character_creator_addr);
	ByteswapHelpers::SWAP_INT32(&movesetHeader->date_addr);
	ByteswapHelpers::SWAP_INT32(&movesetHeader->fulldate_addr);

	for (unsigned int i = 0; i < _countof(movesetHeader->orig_aliases); ++i)
	{
		ByteswapHelpers::SWAP_INT16(&movesetHeader->orig_aliases[i]);
		ByteswapHelpers::SWAP_INT16(&movesetHeader->current_aliases[i]);
	}

	for (unsigned int i = 0; i < _countof(movesetHeader->unknown_aliases); ++i) {
		ByteswapHelpers::SWAP_INT16(&movesetHeader->unknown_aliases[i]);
	}

	// Convert ptrs into offsets
	movesetHeader->character_name_addr -= (gameAddr32)movesetAddr;
	movesetHeader->character_creator_addr -= (gameAddr32)movesetAddr;
	movesetHeader->date_addr -= (gameAddr32)movesetAddr;
	movesetHeader->fulldate_addr -= (gameAddr32)movesetAddr;

	// Correct offsets according to our name modifications
	const uint32_t namelength = (uint32_t)strlen(MOVESET_EXTRACTED_NAME_PREFIX) - 1; // - 1 because we replace one char
	movesetHeader->character_creator_addr += namelength;
	movesetHeader->date_addr += namelength;
	movesetHeader->fulldate_addr += namelength;
}

uint64_t ExtractorTTT2::CalculateMotaCustomBlockSize(const MotaList* motas, std::map<gameAddr32, std::pair<uint32_t, uint32_t>>& offsetMap, ExtractSettings settings)
{
	uint32_t motaCustomBlockSize = 0;

	for (uint16_t motaId = 0; motaId < 12; ++motaId)
	{
		gameAddr32 motaAddr = (gameAddr32)motas->motas[motaId];
		uint32_t motaSize;
		// Motas are listed contigously in two different blocks. The list alternate between one pointer of one block then one pointer to the other. Hnece the i + 2

		if (offsetMap.find(motaAddr) != offsetMap.end()) {
			// Already saved this one, not saving it again
			continue;
		}

		MotaHeader header;
		m_game->ReadBytes(motaAddr, &header, sizeof(MotaHeader));
		if (memcmp(header.mota_string, "MOTA", 4) != 0) {
			// todo
			DEBUG_LOG("Malformed MOTA %d\n", motaId);
			// Malformed MOTA, don't save it
			continue;
		}

		uint32_t animCount = header.anim_count;
		if (header.IsBigEndian()) {
			animCount = BYTESWAP_INT32(animCount);
		}

		// 1st bit = 1st mota. 2nd bit = 2nd mota. And so on...
		// Use bitwise flags to store which one we want to store
		if ((((uint64_t)1 << motaId) & settings) == 0) {
			DEBUG_LOG("Not saving mota %d : not set to be exported.\n", motaId);
			continue;
		}
		uint32_t lastAnimOffset = 0;
		uint32_t* animOffsetList = (uint32_t*)calloc(animCount, sizeof(uint32_t));

		if (animOffsetList == nullptr) {
			DEBUG_LOG("Error while allocating the animation offset list (size %u * 4) for mota %d\n", animCount, motaId);
			throw;
		}

		// Loop through every offset, find the highest
		// There is no guarantee they are ordered so looking through all of them is quite safe
		m_game->ReadBytes(motaAddr + offsetof(MotaHeader, anim_offset_list), animOffsetList, sizeof(uint32_t) * animCount);
		for (size_t animIdx = 0; animIdx < animCount; ++animIdx)
		{
			uint32_t animOffset = animOffsetList[animIdx];
			if (header.IsBigEndian()) {
				animOffset = BYTESWAP_INT32(animOffset);
			}
			if (animOffset > lastAnimOffset) {
				lastAnimOffset = animOffset;
			}
		}

		free(animOffsetList);


		if (lastAnimOffset == 0) {
			DEBUG_LOG("Empty MOTA %d - ", motaId);
			motaSize = 0x14;
		}
		else {
			uint32_t lastAnimSize = (uint32_t)GetAnimationSize(m_game->baseAddr + motaAddr + lastAnimOffset);
			motaSize = lastAnimOffset + lastAnimSize;
		}

		// Store new mota offset & size in keypair map
		offsetMap[motaAddr] = std::pair<uint32_t, uint32_t>(motaCustomBlockSize, motaSize);
		motaCustomBlockSize += motaSize;

		DEBUG_LOG("Saved mota %d, size is %d (0x%x)\n", motaId, motaSize, motaSize);
	}
	return (uint64_t)motaCustomBlockSize;
}

Byte* ExtractorTTT2::AllocateMotaCustomBlock(MotaList* motas, uint64_t& size_out, ExtractSettings settings)
{
	// Custom block contains the mota files we want and does not contain the invalid/unwanted ones

	// Map of GAME_ADDR:<offset, size> motas
	std::map<gameAddr32, std::pair<uint32_t, uint32_t>> offsetMap;
	uint64_t sizeToAllocate = 0;

	size_out = CalculateMotaCustomBlockSize(motas, offsetMap, settings);

	// Allocate 8 bytes minimum. Allocating 0 might cause problem, so this is safer.
	Byte* customBlock = (Byte*)malloc(max(8, size_out));
	if (customBlock == nullptr) {
		size_out = 0;
		return nullptr;
	}

	//12 motas + 1 unknown (still clearly a ptr)
	gameAddr32* motaAddr = (gameAddr32*)motas;
	// In case the same mota is present twice, i'm using this set to avoid exporting it again
	std::set<gameAddr> exportedMotas;
	for (size_t i = 0; i <= 12; ++i)
	{
		if (offsetMap.find(motaAddr[i]) != offsetMap.end())
		{
			auto& [motaOffset, motaSize] = offsetMap[motaAddr[i]];
			if (!exportedMotas.contains(motaAddr[i]))
			{
				MotaHeader* motaPtr = (MotaHeader*)(customBlock + motaOffset);
				m_game->ReadBytes(motaAddr[i], (char*)motaPtr, motaSize);

				DEBUG_LOG("Mota %llu: little endian %d\n", i, motaPtr->is_little_endian);

				if (motaPtr->IsBigEndian()) {
					// Like the game does when they are big endianed, force every MOTA to be little endianed
					// This helps provide a more deterministic extraction
					TAnimUtils::FromMemory::ByteswapMota((Byte*)motaPtr);
				}

				exportedMotas.insert(motaAddr[i]);
			}
			motaAddr[i] = motaOffset;
		}
		else {
			// Set to misisng address for mota block we aren't exporting
			// The importer will recognize this and fill it with a proper value
			motaAddr[i] = MOVESET_ADDR32_MISSING;
		}
	}

	return customBlock;
}

void ExtractorTTT2::GetNamesBlockBounds(const Move* move, uint64_t moveCount, gameAddr& start, gameAddr& end)
{
	uint64_t smallest = (int64_t)move[0].name_addr;
	uint64_t highest = smallest;

	for (size_t moveId = 0; moveId < moveCount; ++moveId)
	{
		uint64_t nameAddr = (uint64_t)move[moveId].name_addr;
		uint64_t animNameAddr = (uint64_t)move[moveId].anim_name_addr;

		if (nameAddr < smallest) {
			smallest = nameAddr;
		}
		else if (nameAddr > highest) {
			highest = nameAddr;
		}

		if (animNameAddr < smallest) {
			smallest = animNameAddr;
		}
		else if (animNameAddr > highest) {
			highest = animNameAddr;
		}
	}

	// Move to the last string's END instead of staying at the start
	gameAddr lastItemEnd = (gameAddr)highest;
	while (m_game->Read<uint8_t>(lastItemEnd) != 0) {
		lastItemEnd += 1;
	}

	start = smallest;
	end = lastItemEnd + 1; // Add 1 to extract the nullbyte too
}


Byte* ExtractorTTT2::CopyAnimations(const Move* movelist, size_t moveCount, uint64_t& size_out, std::map<gameAddr, uint64_t>& offsets)
{
	uint64_t totalSize = 0;
	std::map<gameAddr, uint64_t> animSizes;
	Byte* animationBlock = nullptr;

	// Vector to keep animation order identical regardless of how the game allocates them
	std::vector<gameAddr> addrList;

	// Get unique animation list
	for (size_t i = 0; i < moveCount; ++i)
	{
		gameAddr anim_addr = movelist[i].anim_addr;

		if (std::find(addrList.begin(), addrList.end(), anim_addr) == addrList.end()) {
			// Don't extract same anim twice
			addrList.push_back(anim_addr);
		}
	}

	// Find anim sizes and establish offsets
	for (gameAddr animAddr : addrList)
	{
		uint64_t animSize = GetAnimationSize(m_game->baseAddr + animAddr);

		if (animSize == 0) {
			DEBUG_LOG("Animation address %llx does not have a valid size.\n", animAddr);
			throw;
		}

		offsets[animAddr] = totalSize;
		animSizes[animAddr] = animSize;
		totalSize += animSize;
	}

	// Allocate block
	if (!(animationBlock = (Byte*)malloc(totalSize))) {
		size_out = 0;
		return nullptr;
	}

	// Read animations and write to our block
	Byte* animationBlockCursor = animationBlock;
	for (gameAddr animAddr : addrList)
	{
		int64_t animSize = animSizes[animAddr];
		m_game->ReadBytes(animAddr, animationBlockCursor, animSize);
		animationBlockCursor += animSize;
	}

	size_out = totalSize;
	return animationBlock;
}

void ExtractorTTT2::FillMovesetTables(gameAddr movesetAddr, MovesetTable* table, MovesetTable* offsets)
{
	// Fill table
	DEBUG_LOG("Moveset addr: %llx, table: %llx\n", movesetAddr + 0x140, m_game->baseAddr + movesetAddr + 0x140);
	m_game->ReadBytes(movesetAddr + 0x140, table, sizeof(MovesetTable));

	// Byteswap to little endian
	for (unsigned int i = 0; i < _countof(table->entries); ++i) {
		ByteswapHelpers::SWAP_INT32(&(table->entries[i].listAddr));
		ByteswapHelpers::SWAP_INT32(&(table->entries[i].listCount));
	}

	// Get the address of the first and last list contained within table. This is used to get the bounds of the movesetBlock
	gameAddr tableStartAddr = table->reactions;
	// Convert the list of ptr into a list of offsets relative to the movesetInfoBlock
	memcpy(offsets, table, sizeof(MovesetTable));
	Helpers::convertPtrsToOffsets(offsets, tableStartAddr, 8, sizeof(MovesetTable) / 4 / 2);
}

Byte* ExtractorTTT2::CopyMovesetBlock(gameAddr movesetAddr, uint64_t& size_out, const MovesetTable& table, const MovesetTable* offsets)
{
	gameAddr blockStart = table.reactions;
	gameAddr blockEnd = table.throwCameras + (sizeof(ThrowCamera) * table.throwCamerasCount);
	auto block = allocateAndReadBlock(blockStart, blockEnd, size_out);
	if (block == nullptr) {
		return nullptr;
	}
	convertMovesetDataToLittleEndian(block, offsets);
	return block;
}

char* ExtractorTTT2::CopyNameBlock(gameAddr movesetAddr, uint64_t& size_out, const Move* movelist, uint64_t moveCount, gameAddr& nameBlockStart)
{
	gameAddr nameBlockEnd;
	GetNamesBlockBounds(movelist, moveCount, nameBlockStart, nameBlockEnd);

	// Prefix we apply to recognize movesets we extracted
	const char* namePrefix = MOVESET_EXTRACTED_NAME_PREFIX;
	size_t toCopy = strlen(namePrefix);
	size_t charactersToReplace = 1; // Replace the first [

	unsigned int nameOffset = 0x20C;

	nameBlockStart = movesetAddr + nameOffset - (toCopy - charactersToReplace);
	char* nameBlock = (char*)allocateAndReadBlock(movesetAddr + nameOffset - (toCopy - charactersToReplace), nameBlockEnd, size_out);
	memcpy(nameBlock, namePrefix, toCopy);

	return nameBlock;
}

Byte* ExtractorTTT2::CopyMotaBlocks(gameAddr movesetAddr, uint64_t& size_out, MotaList* motasList, ExtractSettings settings)
{
	m_game->ReadBytes(movesetAddr + 0x1D8, motasList, sizeof(MotaList));

	// Byteswap MOTA addresses
	for (unsigned int i = 0; i < _countof(motasList->motas); ++i) {
		ByteswapHelpers::SWAP_INT32(&motasList->motas[i]);
	}

	return AllocateMotaCustomBlock(motasList, size_out, settings);
}

#pragma warning(push)
#pragma warning(disable:6385)
Byte* ExtractorTTT2::CopyDisplayableMovelist(gameAddr movesetAddr, gameAddr playerAddress, uint64_t& size_out, ExtractSettings settings)
{
	// Default size if we don't actually extract this block
	size_out = 8;

	if (settings & ExtractSettings_DisplayableMovelist)
	{
		/*
		gameAddr managerAddr = m_game->ReadPtrPath("movelist_manager_addr");

		int playerId = m_process->readInt32(playerAddress + m_game->GetValue("playerid_offset"));

		if (playerId == 1) {
			managerAddr += sizeof(MvlManager);
		}

		MvlHead head;
		gameAddr blockStart = m_process->readInt64(managerAddr + offsetof(MvlManager, mvlHead));
		m_process->readBytes(blockStart, &head, sizeof(head));

		const uint64_t s_playableBlock = sizeof(MvlPlayable) * head.playables_count;
		if (head.playables_count == 0) {
			return (Byte*)calloc(1, size_out);
		}
		MvlPlayable* playables = (MvlPlayable*)malloc(s_playableBlock);
		if (playables == nullptr) {
			return nullptr;
		}
		m_process->readBytes(blockStart + head.playables_offset, playables, s_playableBlock);

		// Use the biggest input offset in order to get the end of the movelist
		// Haven't found a better way but it doesn't crash so there probably is nothing after the inputs
		uint64_t biggestInputOffset = 0;
		for (size_t i = 0; i < head.playables_count; ++i)
		{
			uint64_t lastInputOffset = playables[i].input_sequence_offset + sizeof(MvlInput) * playables[i].input_count;
			lastInputOffset += (i * sizeof(MvlPlayable));
			if (lastInputOffset > biggestInputOffset) {
				biggestInputOffset = lastInputOffset;
			}
		}

		free(playables);
		gameAddr blockEnd = blockStart + biggestInputOffset + head.playables_offset;
		Byte* mvlBlock = allocateAndReadBlock(blockStart, blockEnd, size_out);
		if (mvlBlock == nullptr) {
			return nullptr;
		}

		// Correct translation strings offsets
		{
			MvlDisplayable* displayable = (MvlDisplayable*)((uint64_t)mvlBlock + head.displayables_offset);
			for (size_t i = 0; i < head.displayables_count; ++i)
			{
				int32_t absoluteDisplayableOffset = head.displayables_offset + (int32_t)(i * sizeof(MvlDisplayable));
				for (int j = 0; j < _countof(displayable->all_translation_offsets); ++j) {
					int32_t correctedOffset = absoluteDisplayableOffset + displayable->all_translation_offsets[j];
					displayable->all_translation_offsets[j] = correctedOffset;
				}
				++displayable;
			}
		}

		// Convert relative input sequence offsets to absolute IDs
		{
			MvlPlayable* playable = (MvlPlayable*)((uint64_t)mvlBlock + head.playables_offset);
			uint32_t input_sequence_start = head.inputs_offset;
			for (size_t i = 0; i < head.playables_count; ++i)
			{
				uint32_t playable_addr = head.playables_offset + (int32_t)(sizeof(MvlPlayable) * i);
				uint32_t input_sequence_addr = playable_addr + playable->input_sequence_offset;
				uint32_t input_sequence_id = ((uint64_t)input_sequence_addr - input_sequence_start) / sizeof(MvlInput);
				playable->input_sequence_offset = input_sequence_id;
				++playable;
			}
		}

		return mvlBlock;
		*/
		return (Byte*)calloc(1, size_out);
	}
	else {
		return (Byte*)calloc(1, size_out);
	}
}
#pragma warning(pop)

void ExtractorTTT2::FillHeaderInfos(TKMovesetHeader& infos, gameAddr playerAddress, uint64_t customPropertyCount)
{
	infos.flags = 0;
	infos.gameId = m_gameInfo.gameId;
	infos.minorVersion = m_gameInfo.minorVersion;
	infos.characterId = GetCharacterID(playerAddress);
	memset(infos.gameVersionHash, 0, sizeof(infos.gameVersionHash));
	strcpy_s(infos.version_string, sizeof(infos.version_string), MOVESET_VERSION_STRING);
	strcpy_s(infos.origin, sizeof(infos.origin), m_gameInfo.name);
    
    std::string charName = GetPlayerCharacterName(playerAddress);
	strcpy_s(infos.target_character, sizeof(infos.target_character), charName.c_str());
	strcpy_s(infos.orig_character_name, sizeof(infos.orig_character_name), charName.c_str());
    
	infos.date = Helpers::getCurrentTimestamp();
	infos.extraction_date = infos.date;

#ifdef BUILD_TYPE_DEBUG
	// Mark that the moveset was extracted under debug mode
	infos.flags |= MovesetFlags_Debug;
	strcat_s(infos.version_string, sizeof(infos.version_string), "d");
#endif

	uint64_t propertyListSize = customPropertyCount * sizeof(TKMovesetProperty);

	infos.header_size = (uint32_t)Helpers::align8Bytes(sizeof(TKMovesetHeader));
	infos.block_list = infos.header_size + (uint32_t)Helpers::align8Bytes(propertyListSize);
	infos.block_list_size = (uint32_t)_countof(((TKMovesetHeaderBlocks*)0)->blocks);
	infos.moveset_data_start = infos.block_list + (uint32_t)Helpers::align8Bytes(infos.block_list_size * sizeof(uint64_t));
	infos.moveset_data_size = 0;
}

// -- Public methods -- //

ExtractionErrcode_ ExtractorTTT2::Extract(gameAddr playerAddress, ExtractSettings settings, uint8_t& progress)
{
	progress = 0;
	// These are all the blocks we are going to extract. Most of them will be ripped in one big readBytes()
	Byte* headerBlock;
	Byte* customPropertiesBlock;
	Byte* offsetListBlock;
	Byte* movesetInfoBlock;
	Byte* tableBlock;
	Byte* motasListBlock;
	char* nameBlock;
	Byte* movesetBlock;
	Byte* animationBlock; // This is a custom block: we are building it ourselves because animations are not stored contiguously, as opposed to other datas
	Byte* motaCustomBlock; // This is also a custom block because not contiguously stored
	Byte* movelistBlock;

	// The size in bytes of the same blocks
	uint64_t s_headerBlock = sizeof(TKMovesetHeader);
	uint64_t s_customProperties;
	uint64_t s_offsetListBlock = sizeof(TKMovesetHeaderBlocks);
	uint64_t s_movesetInfoBlock = 0x140; // Pretty much the offset of the moveset table
	uint64_t s_tableBlock = sizeof(MovesetTable);
	uint64_t s_motasListBlock = sizeof(MotaList);
	uint64_t s_nameBlock;
	uint64_t s_movesetBlock;
	uint64_t s_animationBlock;
	uint64_t s_motaCustomBlock;
	uint64_t s_movelistBlock;

	// Establish the list of default properties
	TKMovesetProperty customProperties[1] = {
		// END should always be in there to mark the end as the list size isn't stored
		{.id = TKMovesetProperty_END, .value = 0 }
	};
	s_customProperties = sizeof(customProperties);

	// Will contain our own informations such as date, version, character id
	TKMovesetHeader customHeader;
	progress = 5;

	// Will contain the offsets for each block we extract
	TKMovesetHeaderBlocks offsetList{ 0 };

	// The address of the moveset we will be extracting
	gameAddr movesetAddr;
	movesetAddr = m_game->ReadPtr(playerAddress + m_game->GetValue("motbin_offset"));

	// Will read the header of the moveset and write it here
	MovesetInfo movesetHeader{ 0 };

	// We will read the table containing <list_ptr:list_size>, in here.
	// Offsets will contain the same as table but converted to offsets, in order to convert the absolute pointers contained within the moveset to offsets
	MovesetTable table{};
	MovesetTable* offsets = &movesetHeader.table;

	// Contains absolute addresses of mota file within the game's memory
	MotaList motasList{};

	// Assign these blocks right away because they're fixed-size structures we write into
	headerBlock = (Byte*)&customHeader;
	customPropertiesBlock = (Byte*)&customProperties;
	offsetListBlock = (Byte*)&offsetList;
	tableBlock = (Byte*)offsets;
	motasListBlock = (Byte*)&motasList;
	movesetInfoBlock = (Byte*)&movesetHeader;

	// Fill table containing lists of move, lists of cancels, etc...
	FillMovesetTables(movesetAddr, &table, offsets);
	progress = 10;

	// Reads block containing the actual moveset data
	// Also get a pointer to our movelist in our own allocated memory. Will be needing it for animation & names extraction.
	movesetBlock = CopyMovesetBlock(movesetAddr, s_movesetBlock, table, offsets);
	const Move* movelist = (Move*)(movesetBlock + offsets->move);
	if (movesetBlock == nullptr) {
		// Since movesetBlock is used by those Copy functions, we have to check for allocation failure here
		return ExtractionErrcode_AllocationErr;
	}
	progress = 20;

	// movelistBlock
	movelistBlock = CopyDisplayableMovelist(movesetAddr, playerAddress, s_movelistBlock, settings);
	progress = 35;

	// Read mota list, allocate & copy desired mota, prepare anim list to properly guess size of anims later
	motaCustomBlock = CopyMotaBlocks(movesetAddr, s_motaCustomBlock, &motasList, settings);
	progress = 50;

	// Extract animations and build a map for their old address -> their new offset in our blocks
	std::map<gameAddr, uint64_t> animOffsets;
	animationBlock = CopyAnimations(movelist, table.moveCount, s_animationBlock, animOffsets);
	progress = 65;

	// Reads block containing names of moves and animations
	gameAddr nameBlockStart;
	nameBlock = CopyNameBlock(movesetAddr, s_nameBlock, movelist, table.moveCount, nameBlockStart);

	// Reads block containing basic moveset infos and aliases
	CopyMovesetInfoBlock(movesetAddr, &movesetHeader);
	progress = 70;

	// Now that we extracted everything, we can properly convert pts to indexes
	convertMovesetPointersToIndexes(movesetBlock, table, offsets, nameBlockStart, animOffsets);
	progress = 75;

	// -- Extraction & data conversion finished --

	// Setup our own header to write in the output file containg useful information
	std::string characterName = GetPlayerCharacterName(playerAddress);
	progress = 77;

	// Fill the header with our own useful informations
	FillHeaderInfos(customHeader, playerAddress, _countof(customProperties));
	progress = 79;

	// Calculate each offsets according to the previous block offset + its size
	// Offsets are relative to movesetInfoBlock (which is always 0) and not absolute within the file
	// This is because you are not suppoed to allocate the header in the game, header that is stored before movesetInfoBlock
	// 8 bytes alignment isn't strictly needed, but i've had problems in the past on misaligned structures so this is safer
	offsetList.movesetInfoBlock = 0;
	offsetList.tableBlock = Helpers::align8Bytes(offsetList.movesetInfoBlock + s_movesetInfoBlock);
	offsetList.motalistsBlock = Helpers::align8Bytes(offsetList.tableBlock + s_tableBlock);
	offsetList.nameBlock = Helpers::align8Bytes(offsetList.motalistsBlock + s_motasListBlock);
	offsetList.movesetBlock = Helpers::align8Bytes(offsetList.nameBlock + s_nameBlock);
	offsetList.animationBlock = Helpers::align8Bytes(offsetList.movesetBlock + s_movesetBlock);
	offsetList.motaBlock = Helpers::align8Bytes(offsetList.animationBlock + s_animationBlock);
	offsetList.movelistBlock = Helpers::align8Bytes(offsetList.motaBlock + s_motaCustomBlock);

	ExtractionErrcode_ errcode = ExtractionErrcode_Successful;

	// -- Writing the file -- 

	if (movesetInfoBlock == nullptr || nameBlock == nullptr || movesetBlock == nullptr
		|| animationBlock == nullptr || motaCustomBlock == nullptr || movelistBlock == nullptr)
	{
		errcode = ExtractionErrcode_AllocationErr;
		DEBUG_LOG("movesetInfoBlock = %llx\nnameBlock = %llx\nmovesetBlock = %llx\nanimationBlock = %llx\nmotaCustomBlock = %llx\nmovelistBlock = %llx\n",
			(uint64_t)movesetInfoBlock, (uint64_t)nameBlock, (uint64_t)movesetBlock, (uint64_t)animationBlock, (uint64_t)motaCustomBlock, (uint64_t)s_movelistBlock);
	}
	else {
		// Create the file

		std::wstring filepath;
		std::wstring tmp_filepath;

		GetFilepath(characterName.c_str(), filepath, tmp_filepath, (settings & ExtractSettings_OVERWRITE_SAME_FILENAME) != 0);

		std::ofstream file(tmp_filepath.c_str(), std::ios::binary);

		if (!file.fail())
		{
			progress = 80;


			// List of blocks that will be written to the file. Each block is 8 bytes aligned
			std::vector<std::pair<Byte*, uint64_t>> writtenFileBlocks{
				// Header block containing important moveset informations
				{headerBlock, s_headerBlock},
				// Custom block of variable length containing a list of custom properties to apply on import
				{customPropertiesBlock, s_customProperties},
				// Contains the list of moveset data blocks' offsets
				{offsetListBlock, s_offsetListBlock},

				// Actual moveset data start. Accurate up to the animation block
				{movesetInfoBlock, s_movesetInfoBlock},
				{tableBlock, s_tableBlock },
				{motasListBlock, s_motasListBlock},
				{(Byte*)nameBlock, s_nameBlock},
				{movesetBlock, s_movesetBlock},
				{animationBlock, s_animationBlock},
				{motaCustomBlock, s_motaCustomBlock},
				// Displayable movelist block
				{movelistBlock, s_movelistBlock},
			};

			// List of blocks used for the CRC32 calculation. Some blocks above are purposefully ignored.
			// Every block that can be manually modified is in there
			std::vector<std::pair<Byte*, uint64_t>> hashedFileBlocks{
				{customPropertiesBlock, s_customProperties},

				{movesetInfoBlock, s_movesetInfoBlock},
				{tableBlock, s_tableBlock },
				{motasListBlock, s_motasListBlock},
				{movesetBlock, s_movesetBlock},
				{animationBlock, s_animationBlock},
				{motaCustomBlock, s_motaCustomBlock},
			};

			customHeader.crc32 = Helpers::CalculateCrc32(hashedFileBlocks);
			customHeader.orig_crc32 = customHeader.crc32;
			ExtractorUtils::WriteFileData(file, writtenFileBlocks, progress, 95);

			file.close();

			if (settings & ExtractSettings_Compress) {
				DEBUG_LOG("Saved temp moveset, compressing...\n");
				if (!CompressionUtils::FILE::Moveset::Compress(filepath, tmp_filepath, ExtractorUtils::GetCompressionAlgorithm(settings))) {
					errcode = ExtractionErrcode_CompressionFailure;
				}
			}
			else {
				DEBUG_LOG("Not compressing.\n");
				std::filesystem::rename(tmp_filepath, filepath);
			}

			DEBUG_LOG("- Saved moveset at '%S' -\n", filepath.c_str());

			progress = 100;
			// Extraction is over
		}
		else {
			errcode = ExtractionErrcode_FileCreationErr;
		}
	}


	// -- Cleanup -- //

	/// Cleanup our temp allocated memory blocks
	// headerBlock: not allocated, don't free()
	// movesetInfoBlock: not allocated, don't free()
	// tableBlock: not allocated, don't free()
	// motasListBlock: not allocated, don't free()
	free(nameBlock);
	free(movesetBlock);
	free(animationBlock);
	free(motaCustomBlock);
	free(movelistBlock);

	return errcode;
}

bool ExtractorTTT2::CanExtract()
{
	// todo: this is invalid, because when we import our own moveset and leave back to main menu, it will return true
	// yes we can import in that case but it will serve zero purpose
	gameAddr playerAddress = m_game->ReadPtrPath("p1_addr");

	// We'll just read through a bunch of values that wouldn't be valid if a moveset wasn't loaded
	// readInt64() may return -1 if the read fails so we have to check for this value as well.

	if (playerAddress == 0 || playerAddress == -1) {
		return false;
	}

	for (int i = 0; i < 1; ++i)
	{
		gameAddr player = playerAddress + i * m_game->GetValue("playerstruct_size");

		// Read into current moveset to see if it has been initialized
		{
			gameAddr movesetAddr = m_game->ReadPtr(playerAddress + m_game->GetValue("motbin_offset"));

			if (movesetAddr == 0 || movesetAddr == -1) {
				return false;
			}

			int8_t isInitialized = m_game->Read<int8_t>(movesetAddr + offsetof(MovesetInfo, isInitialized));
			if (isInitialized != 1) {
				return false;
			}
		}
	}
	return true;
}


std::string ExtractorTTT2::GetPlayerCharacterName(gameAddr playerAddress)
{
	gameAddr movesetAddr = m_game->ReadPtr(playerAddress + m_game->GetValue("motbin_offset"));

	std::string characterName;
	if (movesetAddr == 0) {
		return characterName;
	}

	{
		char buf[32];
		m_game->ReadBytes(movesetAddr + 0x20C, buf, 31);
		buf[31] = '\0';
		characterName = std::string(buf);
	}

	if (characterName.size() == 0) {
		return std::string();
	}

	// Turn name into something more readable. '[CHARACTER_NAME.s]' becomes 'Character Name.S'
	if (characterName.front() == '[') {
		characterName.erase(0, 1);
	}
	if (characterName.back() == ']') {
		characterName.erase(characterName.size() - 1);
	}

	std::replace(characterName.begin(), characterName.end(), '_', ' ');
	{
		bool isWordStart = true;
		for (size_t i = 0; i < characterName.size(); ++i)
		{
			// Have to pass an unsigned char or this will throw when an invalid character is found
			if (!isprint((unsigned char)characterName[i])) {
				characterName[i] = ' ';
			}
			else if (isalpha((unsigned char)characterName[i])) {
				characterName[i] = isWordStart ? toupper(characterName[i]) : tolower(characterName[i]);
			}
			isWordStart = strchr(" -.:", characterName[i]) != nullptr;
		}
	}
	return characterName;
}

uint32_t ExtractorTTT2::GetCharacterID(gameAddr playerAddress)
{
	return m_game->Read<uint32_t>(playerAddress + m_game->GetValue("chara_id_offset"));
}

gameAddr ExtractorTTT2::GetCharacterAddress(uint8_t playerId)
{
	gameAddr playerAddress = m_game->ReadPtrPath("p1_addr");
	if (playerId > 0) {
		playerAddress += playerId * m_game->GetValue("playerstruct_size");
	}
	return playerAddress;
}

std::vector<gameAddr> ExtractorTTT2::GetCharacterAddresses()
{
	gameAddr playerAddress = m_game->ReadPtrPath("p1_addr");
	uint64_t playerstructSize = m_game->GetValue("playerstruct_size");
	std::vector<gameAddr> addresses;

	for (uint8_t i = 0; i < m_gameInfo.characterCount; ++i) {
		addresses.push_back(playerAddress + i * playerstructSize);
	}

	return addresses;
}