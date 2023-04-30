#include <set>
#include <string>
#include <cctype>
#include <stddef.h>
#include <iterator>
#include <vector>
#include <algorithm>
#include <filesystem>

#include "helpers.hpp"
#include "Extractor_t7.hpp"

using namespace StructsT7;

// Contains the same structure as StructsT7 but with gameAddr types instead of ptrs types
// Defined here because i don't want any other file to have access to this shortcut
#define gAddr StructsT7_gameAddr

// -- Static helpers -- //

// Converts absolute ptr into indexes before saving to file
static void convertMovesetPointersToIndexes(Byte* movesetBlock, const gAddr::MovesetTable& table, const gAddr::MovesetTable* offsets, gameAddr nameStart, std::map<gameAddr, uint64_t>& animOffsetMap)
{
	// Convert move ptrs
	for (auto& move : StructIterator<gAddr::Move>(movesetBlock, offsets->move, table.moveCount))
	{
		move.name_addr -= nameStart;
		move.anim_name_addr -= nameStart;
		move.anim_addr = animOffsetMap[move.anim_addr];
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

	// Convert projectile ptrs
	for (auto& projectile : StructIterator<gAddr::Projectile>(movesetBlock, offsets->projectile, table.projectileCount))
	{
		// One projectile actually has both at 0 for some reason ?
		TO_INDEX(projectile.cancel_addr, table.cancel, Cancel);
		TO_INDEX(projectile.hit_condition_addr, table.hitCondition, HitCondition);
	}

	// Convert cancel ptrs
	for (auto& cancel : StructIterator<gAddr::Cancel>(movesetBlock, offsets->cancel, table.cancelCount))
	{
		TO_INDEX(cancel.requirements_addr, table.requirement, Requirement);
		TO_INDEX(cancel.extradata_addr, table.cancelExtradata, CancelExtradata);
	}

	// Convert groupCancel cancel ptrs
	for (auto& groupCancel : StructIterator<gAddr::Cancel>(movesetBlock, offsets->groupCancel, table.groupCancelCount))
	{
		TO_INDEX(groupCancel.requirements_addr, table.requirement, Requirement);
		TO_INDEX(groupCancel.extradata_addr, table.cancelExtradata, CancelExtradata);
	}

	// Convert reaction ptrs
	for (auto& reaction : StructIterator<gAddr::Reactions>(movesetBlock, offsets->reactions, table.reactionsCount))
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
	for (auto& inputSequence : StructIterator<gAddr::InputSequence>(movesetBlock, offsets->inputSequence, table.inputSequenceCount))
	{
		TO_INDEX(inputSequence.input_addr, table.input, Input);
	}

	// Convert throwCameras ptrs
	for (auto& throwCameras : StructIterator<gAddr::ThrowCamera>(movesetBlock, offsets->throwCameras, table.throwCamerasCount))
	{
		TO_INDEX(throwCameras.cameradata_addr, table.cameraData, CameraData);
	}

	// Convert hit conditions ptrs
	for (auto& hitCondition : StructIterator<gAddr::HitCondition>(movesetBlock, offsets->hitCondition, table.hitConditionCount))
	{
		TO_INDEX(hitCondition.requirements_addr, table.requirement, Requirement);
		TO_INDEX(hitCondition.reactions_addr, table.reactions, Reactions);
	}

	// Convert pushback ptrs
	for (auto& pushback : StructIterator<gAddr::Pushback>(movesetBlock, offsets->pushback, table.pushbackCount))
	{
		TO_INDEX(pushback.extradata_addr, table.pushbackExtradata, PushbackExtradata);
	}

	// Convert move start prop ptrs
	for (auto& moveBeginningProp : StructIterator<gAddr::OtherMoveProperty>(movesetBlock, offsets->moveBeginningProp, table.moveBeginningPropCount))
	{
		TO_INDEX(moveBeginningProp.requirements_addr, table.requirement, Requirement);
	}

	// Convert move end prop ptrs
	for (auto& moveEndingProp : StructIterator<gAddr::OtherMoveProperty>(movesetBlock, offsets->moveEndingProp, table.moveEndingPropCount))
	{
		TO_INDEX(moveEndingProp.requirements_addr, table.requirement, Requirement);
	}
}

// -- Private methods - //

void ExtractorT7::CopyMovesetInfoBlock(gameAddr movesetAddr, gAddr::MovesetInfo* movesetHeader)
{
	m_process->readBytes(movesetAddr, movesetHeader, offsetof(MovesetInfo, table));

	// Convert ptrs into offsets
	movesetHeader->character_name_addr -= movesetAddr;
	movesetHeader->character_creator_addr -= movesetAddr;
	movesetHeader->date_addr -= movesetAddr;
	movesetHeader->fulldate_addr -= movesetAddr;

	// Correct offsets according to our name modifications
	const size_t namelength = strlen(MOVESET_EXTRACTED_NAME_PREFIX) - 1; // - 1 because we replace one char
	movesetHeader->character_creator_addr += namelength;
	movesetHeader->date_addr += namelength;
	movesetHeader->fulldate_addr += namelength;
}

uint64_t ExtractorT7::CalculateMotaCustomBlockSize(const MotaList* motas, std::map<gameAddr, std::pair<gameAddr, uint64_t>>& offsetMap, ExtractSettings settings)
{
	uint64_t motaCustomBlockSize = 0;

	for (uint16_t motaId = 0; motaId < 12; ++motaId)
	{
		gameAddr motaAddr = (gameAddr)motas->motas[motaId];
		gameAddr motaSize;
		// Motas are listed contigously in two different blocks. The list alternate between one pointer of one block then one pointer to the other. Hnece the i + 2

		if (offsetMap.find(motaAddr) != offsetMap.end()) {
			// Already saved this one, not saving it again
			continue;
		}

		MotaHeader header;
		m_process->readBytes(motaAddr, &header, sizeof(MotaHeader));
		if (memcmp(header.mota_string, "MOTA", 4) != 0) {
			DEBUG_LOG("Malformed MOTA %d\n", motaId);
			// Malformed MOTA, don't save it
			continue;
		}

		uint32_t animCount = header.anim_count;
		if (header.is_big_endian) {
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
		m_process->readBytes(motaAddr + offsetof(MotaHeader, anim_offset_list), animOffsetList, sizeof(uint32_t) * animCount);
		for (size_t animIdx = 0; animIdx < animCount; ++animIdx)
		{
			uint32_t animOffset = animOffsetList[animIdx];
			if (header.is_big_endian) {
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
			uint64_t lastAnimSize = GetAnimationSize(motaAddr + lastAnimOffset);
			motaSize = lastAnimOffset + lastAnimSize;
		}

		// Store new mota offset & size in keypair map
		offsetMap[motaAddr] = std::pair<gameAddr, uint64_t>(motaCustomBlockSize, motaSize);
		motaCustomBlockSize += motaSize;

		DEBUG_LOG("Saved mota %d, size is %lld (0x%llx)\n", motaId, motaSize, motaSize);
	}
	return motaCustomBlockSize;
}

Byte* ExtractorT7::AllocateMotaCustomBlock(MotaList* motas, uint64_t& size_out, ExtractSettings settings)
{
	// Custom block contains the mota files we want and does not contain the invalid/unwanted ones

	// Map of GAME_ADDR:<offset, size> motas
	std::map<gameAddr, std::pair<gameAddr, uint64_t>> offsetMap;
	uint64_t sizeToAllocate = 0;

	size_out = CalculateMotaCustomBlockSize(motas, offsetMap, settings);

	// Allocate 8 bytes minimum. Allocating 0 might cause problem, so this is safer.
	Byte* customBlock = (Byte*)malloc(max(8, size_out));
	if (customBlock == nullptr) {
		size_out = 0;
		return nullptr;
	}
	
	//12 motas + 1 unknown (still clearly a ptr)
	gameAddr* motaAddr = (gameAddr*)motas;
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
				m_process->readBytes(motaAddr[i], (char*)motaPtr, motaSize);

				if (motaPtr->is_big_endian) {
					ExtractorUtils::ByteswapMota((Byte*)motaPtr);
				}

				exportedMotas.insert(motaAddr[i]);
			}
			motaAddr[i] = motaOffset;
		}
		else {
			// Set to misisng address for mota block we aren't exporting
			// The importer will recognize this and fill it with a proper value
			motaAddr[i] = MOVESET_ADDR_MISSING;
		}
	}

	return customBlock;
}

void ExtractorT7::GetNamesBlockBounds(const gAddr::Move* move, uint64_t moveCount, gameAddr& start, gameAddr& end)
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
	while (m_process->readInt8(lastItemEnd) != 0) {
		lastItemEnd += 1;
	}

	start = smallest;
	end = lastItemEnd + 1; // Add 1 to extract the nullbyte too
}


Byte* ExtractorT7::CopyAnimations(const gAddr::Move* movelist, size_t moveCount, uint64_t &size_out, std::map<gameAddr, uint64_t>& offsets)
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
		uint64_t animSize = GetAnimationSize(animAddr);

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
		m_process->readBytes(animAddr, animationBlockCursor, animSize);
		animationBlockCursor += animSize;
	}

	size_out = totalSize;
	return animationBlock;
}

void ExtractorT7::FillMovesetTables(gameAddr movesetAddr, gAddr::MovesetTable* table, gAddr::MovesetTable* offsets)
{
	// Fill table
	m_process->readBytes(movesetAddr + 0x150, table, sizeof(MovesetTable));
	// Get the address of the first and last list contained within table. This is used to get the bounds of the movesetBlock
	gameAddr tableStartAddr = (gameAddr)table->reactions;
	// Convert the list of ptr into a list of offsets relative to the movesetInfoBlock
	memcpy(offsets, table, sizeof(MovesetTable));
	Helpers::convertPtrsToOffsets(offsets, tableStartAddr, 16, sizeof(MovesetTable) / 8 / 2);
}

Byte* ExtractorT7::CopyMovesetBlock(gameAddr movesetAddr, uint64_t& size_out, const gAddr::MovesetTable& table)
{
	gameAddr blockStart = (gameAddr)table.reactions;
	gameAddr blockEnd = (gameAddr)table.throwCameras + (sizeof(ThrowCamera) * table.throwCamerasCount);
	return allocateAndReadBlock(blockStart, blockEnd, size_out);
}

char* ExtractorT7::CopyNameBlock(gameAddr movesetAddr, uint64_t& size_out, const gAddr::Move* movelist, uint64_t moveCount, gameAddr& nameBlockStart)
{
	gameAddr nameBlockEnd;
	GetNamesBlockBounds(movelist, moveCount, nameBlockStart, nameBlockEnd);

	// Prefix we apply to recognize movesets we extracted
	const char* namePrefix = MOVESET_EXTRACTED_NAME_PREFIX;
	size_t toCopy = strlen(namePrefix);
	size_t charactersToReplace = 1; // Replace the first [

	nameBlockStart = movesetAddr + 0x2E8 - (toCopy - charactersToReplace);
	char* nameBlock = (char*)allocateAndReadBlock(movesetAddr + 0x2E8 - (toCopy - charactersToReplace), nameBlockEnd, size_out);
	memcpy(nameBlock, namePrefix, toCopy);

	return nameBlock;
}

Byte* ExtractorT7::CopyMotaBlocks(gameAddr movesetAddr, uint64_t& size_out, MotaList* motasList, ExtractSettings settings)
{
	m_process->readBytes(movesetAddr + 0x280, motasList, sizeof(MotaList));
	return AllocateMotaCustomBlock(motasList, size_out, settings);
}

Byte* ExtractorT7::CopyDisplayableMovelist(gameAddr movesetAddr, gameAddr playerAddress, uint64_t& size_out, ExtractSettings settings)
{
	// Default size if we don't actually extract this block
	size_out = 8;

	if (settings & ExtractSettings_DisplayableMovelist)
	{
		gameAddr managerAddr = m_game->ReadPtr("movelist_manager_addr");

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

		{
			MvlPlayable* playable = (MvlPlayable*)((uint64_t)mvlBlock + head.playables_offset);
			uint32_t input_sequence_start = head.inputs_offset;
			for (size_t i = 0; i < head.playables_count; ++i)
			{
				uint32_t playable_addr = head.playables_offset + (int32_t)(sizeof(MvlPlayable) * i);
				uint32_t input_sequence_addr = playable_addr + playable->input_sequence_offset;
				uint32_t input_sequence_id = (input_sequence_addr - input_sequence_start) / sizeof(MvlInput);
				playable->input_sequence_offset = input_sequence_id;
				++playable;
			}
		}

		return mvlBlock;
	}
	else {
		return (Byte*)calloc(1, size_out);
	}
}

void ExtractorT7::FillHeaderInfos(TKMovesetHeader& infos, gameAddr playerAddress, uint64_t customPropertyCount)
{
	infos.flags = 0;
	infos.gameId = m_gameInfo.gameId;
	infos.minorVersion = m_gameInfo.minorVersion;
	infos.characterId = GetCharacterID(playerAddress);
	memset(infos.gameVersionHash, 0, sizeof(infos.gameVersionHash));
	strcpy_s(infos.version_string, sizeof(infos.version_string), MOVESET_VERSION_STRING);
	strcpy_s(infos.origin, sizeof(infos.origin), m_gameInfo.name);
	strcpy_s(infos.target_character, sizeof(infos.target_character), GetPlayerCharacterName(playerAddress).c_str());
	infos.date = Helpers::getCurrentTimestamp();
	infos.extraction_date = infos.date;

	uint64_t propertyListSize = customPropertyCount * sizeof(TKMovesetProperty);

	infos.header_size = (uint32_t)Helpers::align8Bytes(sizeof(TKMovesetHeader));
	infos.block_list = infos.header_size + (uint32_t)Helpers::align8Bytes(propertyListSize);
	infos.block_list_size = (uint32_t)_countof(((TKMovesetHeaderBlocks*)0)->blocks);
	infos.moveset_data_start = infos.block_list + (uint32_t)Helpers::align8Bytes(infos.block_list_size * sizeof(uint64_t));
	infos.moveset_data_size = 0;
}

// -- Public methods -- //

ExtractionErrcode_ ExtractorT7::Extract(gameAddr playerAddress, ExtractSettings settings, uint8_t& progress)
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
	uint64_t s_movesetInfoBlock = 0x150; // Yeah, fixed size, this is on purpose. I do want to extract table and mota separately.
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
	movesetAddr = m_process->readInt64(playerAddress + m_game->GetValue("motbin_offset"));

	// Will read the header of the moveset and write it here
	gAddr::MovesetInfo movesetHeader{ 0 };

	// We will read the table containing <list_ptr:list_size>, in here.
	// Offsets will contain the same as table but converted to offsets, in order to convert the absolute pointers contained within the moveset to offsets
	gAddr::MovesetTable table{};
	gAddr::MovesetTable* offsets = &movesetHeader.table;

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
	movesetBlock = CopyMovesetBlock(movesetAddr, s_movesetBlock, table);
	const gAddr::Move* movelist = (gAddr::Move*)(movesetBlock + offsets->move);
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
				if (!ExtractorUtils::CompressFile(customHeader.moveset_data_start, filepath, tmp_filepath)) {
					errcode = ExtractionErrcode_CompressionFailure;
				}
			}
			else {
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

bool ExtractorT7::CanExtract()
{
	// todo: this is invalid, because when we import our own moveset and leave back to main menu, it will return true
	// yes we can import in that case but it will serve zero purpose
	gameAddr playerAddress = m_game->ReadPtr("p1_addr");
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
			gameAddr movesetAddr = m_process->readUInt64(playerAddress + m_game->GetValue("motbin_offset"));
			if (movesetAddr == 0 || movesetAddr == -1) {
				return false;
			}

			int8_t isInitialized = m_process->readInt8(movesetAddr + offsetof(MovesetInfo, isInitialized));
			if (isInitialized != 1) {
				return false;
			}

			// Todo: Some mota are converted to little endian when movesets are loaded in the game
			// Some aren't, find which, to avoid bad exports
			/*
			uint64_t listStart = offsetof(MovesetInfo, motas);
			for (size_t motaId = 0; motaId < _countof(((MotaList*)0)->motas); ++motaId)
			{
				gameAddr motaAddr = m_process->readUInt64(movesetAddr + listStart + (8 * motaId));
				if (motaAddr == 0 || motaAddr == -1) {
					return false;
				}

				auto mota_swapped = m_process->readInt16(motaAddr + offsetof(MotaHeader, is_little_endian));
			}
			*/
		}

		// Read into current move to see if it is valid
		{
			gameAddr currentMove = m_process->readUInt64(player + m_game->GetValue("currmove"));
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
	}
	return true;
}


std::string ExtractorT7::GetPlayerCharacterName(gameAddr playerAddress)
{
	gameAddr movesetAddr = m_process->readInt64(playerAddress + m_game->GetValue("motbin_offset"));

	std::string characterName;
	if (movesetAddr == 0) {
		return characterName;
	}

	{
		char buf[32]{ 0 };
		m_process->readBytes(movesetAddr + 0x2E8, buf, 31);
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
			} else if (isalpha((unsigned char)characterName[i])) {
				characterName[i] = isWordStart ? toupper(characterName[i]) : tolower(characterName[i]);
			}
			isWordStart = strchr(" -.:", characterName[i]) != nullptr;
		}
	}
	return characterName;
}

uint32_t ExtractorT7::GetCharacterID(gameAddr playerAddress)
{
	return m_process->readInt16(playerAddress + m_game->GetValue("chara_id_offset"));
}

gameAddr ExtractorT7::GetCharacterAddress(uint8_t playerId)
{
	gameAddr playerAddress = m_game->ReadPtr("p1_addr");
	if (playerId > 0) {
		playerAddress += playerId * m_game->GetValue("playerstruct_size");
	}
	return playerAddress;
}

std::vector<gameAddr> ExtractorT7::GetCharacterAddresses()
{
	gameAddr playerAddress = m_game->ReadPtr("p1_addr");
	uint64_t playerstructSize = m_game->GetValue("playerstruct_size");
	std::vector<gameAddr> addresses;

	for (uint8_t i = 0; i < characterCount; ++i) {
		addresses.push_back(playerAddress + i * playerstructSize);
	}

	return addresses;
}