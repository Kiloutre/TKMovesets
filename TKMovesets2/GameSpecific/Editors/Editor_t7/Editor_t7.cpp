#include <map>
#include <format>
#include <fstream>

#include "Compression.hpp"
#include "helpers.hpp"
#include "Importer.hpp"
#include "Editor_t7.hpp"
#include "Animations.hpp"

using namespace EditorUtils;

#define gAddr StructsT7_gameAddr

// Utils

namespace EditorT7Utils
{
	const std::map<unsigned char, std::string> mvlDisplay_characterConversion = {
		{ 0x80, "{DB}" },
		{ 0x81, "{D}" },
		{ 0x82, "{DF}" },
		{ 0x83, "{B}" },
		{ 0x84, "{F}" },
		{ 0x85, "{UB}" },
		{ 0x86, "{U}" },
		{ 0x87, "{UF}" },

		{ 0x88, "{!DB}" },
		{ 0x89, "{!D}" },
		{ 0x8A, "{!DF}" },
		{ 0x8B, "{!B}" },
		{ 0x8C, "{!F}" },
		{ 0x8D, "{!UB}" },
		{ 0x8E, "{!U}" },
		{ 0x8F, "{!UF}" },

		{ 0x95, "{1}" },
		{ 0x96, "{2}" },
		{ 0x97, "{12}" },
		{ 0x98, "{3}" },
		{ 0x99, "{13}" },
		{ 0x9A, "{23}" },
		{ 0x9B, "{123}" },
		{ 0x9C, "{4}" },
		{ 0x9D, "{14}" },
		{ 0x9E, "{24}" },
		{ 0x9F, "{124}" },
		{ 0xA0, "{34}" },
		{ 0xA1, "{134}" },
		{ 0xA2, "{234}" },
		{ 0xA3, "{1234}" },

		{ 0xB4, "{N}" },
	};

	std::string ConvertMovelistDisplayableTextToGameText(const char* str)
	{
		std::string result;

		size_t maxlen = strlen(str);
		for (int i = 0; str[i]; ++i)
		{
			if (str[i] == '{')
			{
				int start = i;
				while (str[i] && str[i] != '}') {
					++i;
				}

				if (str[i] == '}') {
					int keyLen = (i - start) + 1;
					bool found = false;

					if (keyLen == 3)
					{
						char prevChar = str[i - 1];
						found = true;

						switch (prevChar)
						{
						case '[':
							result += "\xE3\x80\x90";
							break;
						case ']':
							result += "\xE3\x80\x91";
							break;
						case '>':
							result += "\xEE\x81\xA3";
							break;
						case '<':
							result += "\xEE\x81\xA2";
							break;
						case 'X':
							result += "\xE2\x96\xA0";
							break;
						default:
							found = false;
							break;
						}
					}

					if (!found)
					{
						for (auto& [key, value] : EditorT7Utils::mvlDisplay_characterConversion)
						{
							if (value.size() == keyLen && strncmp(value.c_str(), &str[start], keyLen) == 0)
							{
								unsigned char specialChar[4] = { 0xEE, 0x80, (unsigned char)key, 0 };
								result += (char*)specialChar;
								found = true;
								break;
							}
						}
					}

					if (found) {
						continue;
					}

					// If not found, come back and display the {XX} characters as regular text
					i = start;
				}
			}

			result += str[i];
		}

		return result;
	}
};

// Fixup //

uint64_t EditorT7::CreateMoveName(const char* moveName)
{
	const size_t moveNameSize = strlen(moveName) + 1;

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	// Find position where to insert new name
	uint64_t moveNameOffset = m_header->moveset_data_start + m_offsets->movesetBlock;
	const uint64_t orig_moveNameEndOffset = moveNameOffset;
	while (*(m_moveset + (moveNameOffset - 2)) == 0)
	{
		// Have to find the insert offset backward because the name block is always aligned to 8 bytes
		// We want to erase as many empty bytes because of past alignment and then re-align to 8 bytes
		moveNameOffset--;
	}

	const uint64_t relativeMoveNameOffset = moveNameOffset - m_offsets->nameBlock - m_header->moveset_data_start;
	const uint64_t moveNameEndOffset = Helpers::align8Bytes(moveNameOffset + moveNameSize);

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = moveNameEndOffset + (m_movesetSize - orig_moveNameEndOffset);
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return 0;
	}

	// Shift offsets now that we know the extra allocated size. Need to do it before LoadMovesetPtr.
	const uint64_t extraNameSize = moveNameEndOffset - orig_moveNameEndOffset;

	for (unsigned int i = 0; i < m_header->block_list_size; ++i)
	{
		if ((m_header->moveset_data_start + m_offsets->blocks[i]) >= orig_moveNameEndOffset)
		{
			m_offsets->blocks[i] += extraNameSize;
			DEBUG_LOG("Shifted moveset block %d by 0x%llx\n", i, extraNameSize);
		}
	}

	// Copy start //

	memcpy(newMoveset, m_moveset, moveNameOffset);

	// Write our new name
	memcpy(newMoveset + moveNameOffset, moveName, moveNameSize);

	// Copy all the data after our name
	memcpy(newMoveset + moveNameEndOffset, m_moveset + orig_moveNameEndOffset, m_movesetSize - orig_moveNameEndOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	return relativeMoveNameOffset;
}


// ===== Other ===== //

void EditorT7::SetupIterators_DisplayableMovelist()
{
	m_iterators.mvl_displayables.Set(m_mvlHead, m_mvlHead->displayables_offset, m_mvlHead->displayables_count);
	m_iterators.mvl_playables.Set(m_mvlHead, m_mvlHead->playables_offset, m_mvlHead->playables_count);

	int mvl_inputs_count = 0;
	for (auto& playable : m_iterators.mvl_playables)
	{
		int last_input_offset = playable.input_sequence_offset + playable.input_count;
		if (last_input_offset > mvl_inputs_count) {
			mvl_inputs_count = last_input_offset;
		}
	}
	m_iterators.mvl_inputs.Set(m_mvlHead, m_mvlHead->inputs_offset, mvl_inputs_count);
}

void EditorT7::LoadMovesetPtr(Byte* t_moveset, uint64_t t_movesetSize)
{
	DEBUG_LOG("LoadMovesetPtr(%p, %llu / h%llx)\n", t_moveset, t_movesetSize, t_movesetSize);
	m_moveset = t_moveset;
	m_movesetSize = t_movesetSize;

	// Start getting pointers toward useful data structures
	// Also get the actual game-moveset (past our header) pointer
	m_header = (TKMovesetHeader*)t_moveset;
	m_offsets = (TKMovesetHeaderBlocks*)(t_moveset + m_header->block_list);
	m_movesetData = ((Byte*)t_moveset) + (m_header->moveset_data_start);
	m_movesetDataSize = m_movesetSize - m_header->moveset_data_start;
	m_infos = (MovesetInfo*)m_movesetData;

	// Update our useful iterators
	uint64_t movesetBlock = (uint64_t)m_movesetData + m_offsets->movesetBlock;

	m_iterators.moves.Set(movesetBlock, m_infos->table.move, m_infos->table.moveCount);
	m_iterators.requirements.Set(movesetBlock, m_infos->table.requirement, m_infos->table.requirementCount);
	m_iterators.hit_conditions.Set(movesetBlock, m_infos->table.hitCondition, m_infos->table.hitConditionCount);
	m_iterators.cancels.Set(movesetBlock, m_infos->table.cancel, m_infos->table.cancelCount);
	m_iterators.grouped_cancels.Set(movesetBlock, m_infos->table.groupCancel, m_infos->table.groupCancelCount);
	m_iterators.reactions.Set(movesetBlock, m_infos->table.reactions, m_infos->table.reactionsCount);
	m_iterators.pushbacks.Set(movesetBlock, m_infos->table.pushback, m_infos->table.pushbackCount);
	m_iterators.pushback_extras.Set(movesetBlock, m_infos->table.pushbackExtradata, m_infos->table.pushbackExtradataCount);
	m_iterators.cancel_extras.Set(movesetBlock, m_infos->table.cancelExtradata, m_infos->table.cancelExtradataCount);
	m_iterators.extra_move_properties.Set(movesetBlock, m_infos->table.extraMoveProperty, m_infos->table.extraMovePropertyCount);
	m_iterators.move_start_properties.Set(movesetBlock, m_infos->table.moveBeginningProp, m_infos->table.moveBeginningPropCount);
	m_iterators.move_end_properties.Set(movesetBlock, m_infos->table.moveEndingProp, m_infos->table.moveEndingPropCount);
	m_iterators.projectiles.Set(movesetBlock, m_infos->table.projectile, m_infos->table.projectileCount);
	m_iterators.input_sequences.Set(movesetBlock, m_infos->table.inputSequence, m_infos->table.inputSequenceCount);
	m_iterators.inputs.Set(movesetBlock, m_infos->table.input, m_infos->table.inputCount);
	m_iterators.voiceclips.Set(movesetBlock, m_infos->table.voiceclip, m_infos->table.voiceclipCount);
	m_iterators.throw_datas.Set(movesetBlock, m_infos->table.throwCameras, m_infos->table.throwCamerasCount);
	m_iterators.camera_datas.Set(movesetBlock, m_infos->table.cameraData, m_infos->table.cameraDataCount);

	if (hasDisplayableMovelist) {
		m_mvlHead = (MvlHead*)(m_movesetData + m_offsets->movelistBlock);
		SetupIterators_DisplayableMovelist();
	}

	// Because we re-allocated, tell the live editor that the old moveset in the game memory is now invalid
	live_loadedMoveset = 0;
}

bool EditorT7::LoadMoveset(Byte* t_moveset, uint64_t t_movesetSize)
{
	constants = {
		{EditorConstants_RequirementEnd, (unsigned int)m_game.GetValue("req_end")},
		{EditorConstants_CancelCommandEnd, 0x8000},
		{EditorConstants_ExtraProperty_Instant, 32769}, // 0x8001
		{EditorConstants_ExtraPropertyEnd, 0},
		{EditorConstants_GroupedCancelCommand, 0x800B},
		{EditorConstants_GroupedCancelCommandEnd, 0x800C},
		{EditorConstants_InputSequenceCommandStart, 0x800D},
		{EditorConstants_ProjectileProperty, 0x820B},
	};

	{
		// Uncompress if needed
		TKMovesetHeader* header = (TKMovesetHeader*)t_moveset;
		if (header->isCompressed()) {
			// Compressed moveset data, must decompress
			Byte* new_moveset = t_moveset;
			int32_t compressed_data_size = (int32_t)(t_movesetSize - header->moveset_data_start);

			new_moveset = CompressionUtils::RAW::Moveset::DecompressWithHeader(new_moveset, compressed_data_size, t_movesetSize);
			delete[] t_moveset;

			if (new_moveset == nullptr) {
				return false;
			}

			t_moveset = new_moveset;
		}
	}

	LoadMovesetPtr(t_moveset, t_movesetSize);

	if ((m_header->flags & MovesetFlags_MovesetModified) == 0) {
		// First time this moveset is edited
		strcat_s(m_header->origin, sizeof(m_header->origin), " (M)");
		m_header->flags |= MovesetFlags_MovesetModified;
	}

	// Get aliases as a vector
	uint16_t* aliasesPtr = m_infos->orig_aliases;
	for (uint16_t i = 0; i < _countof(m_infos->orig_aliases); ++i) {
		aliases.push_back(aliasesPtr[i]);
	}

	if ((m_offsets->movelistBlock + 4) < m_movesetDataSize && \
		strncmp((char*)m_movesetData + m_offsets->movelistBlock, "MVLT", 4) == 0)
	{
		m_mvlHead = (MvlHead*)(m_movesetData + m_offsets->movelistBlock);
		hasDisplayableMovelist = true;
		SetupIterators_DisplayableMovelist();
	}

	// Build anim name : offset list
	uint64_t movesetListOffset = m_offsets->movesetBlock + (uint64_t)m_infos->table.move;
	gAddr::Move* movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);
	char const* namePtr = (char const*)(m_movesetData + m_offsets->nameBlock);

	for (size_t i = 0; i < m_infos->table.moveCount; ++i)
	{
		const char* animName = namePtr + movePtr[i].anim_name_addr;
		std::string animName_str(animName);
		gameAddr animOffset = movePtr[i].anim_addr;

		if (m_animNameToOffsetMap.find(animName_str) != m_animNameToOffsetMap.end() && m_animNameToOffsetMap.at(animName_str) != animOffset) {
			// Same animation name refers to a different offset. Create a unique animation name for it.
			// Move names being similar is irrelevant, but i build an anim name -> anim offset map, so i need identical names to point toward the same anim.

			animName_str += " (2)";
			unsigned int num = 2;
			while (m_animNameToOffsetMap.find(animName_str) != m_animNameToOffsetMap.end()) {
				animName_str = std::format("{} ({})", animName, ++num);
			}

			uint64_t newNameOffset = CreateMoveName(animName_str.c_str());
			if (newNameOffset != 0) {
				// Reallocation was done, update the pointers we are using
				movesetListOffset = m_offsets->movesetBlock + (uint64_t)m_infos->table.move;
				movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);
				namePtr = (char const*)(m_movesetData + m_offsets->nameBlock);
				movePtr[i].anim_name_addr = newNameOffset;
				animName = namePtr + movePtr[i].anim_name_addr;
			}
			else {
				// Could not generate new name
				throw std::exception();
			}
		}

		m_animNameToOffsetMap[animName_str] = animOffset;
		m_animOffsetToNameOffset[animOffset] = movePtr[i].anim_name_addr;
	}

	return true;
}

void EditorT7::RecomputeDisplayableMoveFlags(uint16_t moveId)
{
	auto move = m_iterators.moves[moveId];
	char const* namePtr = (char const*)(m_movesetData + m_offsets->nameBlock);

	std::string moveName = std::string(namePtr + move->name_addr);

	EditorMoveFlags flags = 0;

	if (move->hitlevel || move->hitbox_location || move->first_active_frame || move->last_active_frame) {
		if (move->hitlevel && move->hitbox_location && move->first_active_frame && move->last_active_frame) {
			flags |= EditorMoveFlags_Attack;
		}
		else {
			flags |= EditorMoveFlags_OtherAttack;
		}
	}

	if (Helpers::endsWith<std::string>(moveName, "_y")) {
		flags |= EditorMoveFlags_Throw;
	}
	else if (Helpers::endsWith<std::string>(moveName, "_n")) {
		flags |= EditorMoveFlags_ThrowReaction;
	}

	if (Helpers::startsWith<std::string>(moveName, MOVESET_CUSTOM_MOVE_NAME_PREFIX)) {
		flags |= EditorMoveFlags_Custom;
	}

	uint16_t aliasId = 0;
	auto displayedMove = displayableMovelist[moveId];

	// Mark if default generic moves
	{
		uint16_t* aliases = m_infos->orig_aliases;
		uint8_t aliasesCount = (uint8_t)_countof(m_infos->orig_aliases);

		for (uint8_t j = 0; j < aliasesCount; ++j)
		{
			uint16_t aliasMoveId = aliases[j];
			uint16_t aliasId = 0x8000 + j;

			if (moveId != aliasMoveId) {
				continue;
			}

			displayedMove->flags |= EditorMoveFlags_Generic;

			if (displayedMove->aliasId == 0)
			{
				displayedMove->color |= TEditorUtils::GetMoveColorFromFlag(displayedMove->flags);
				aliasId = aliasId;
			}
		}
	}

	// Mark if current generic move
	{
		uint16_t* aliases = m_infos->current_aliases;
		uint8_t aliasesCount = (uint8_t)_countof(m_infos->current_aliases);

		for (uint8_t j = 0; j < aliasesCount; ++j)
		{
			uint16_t aliasMoveId = aliases[j];
			uint16_t aliasId = 0x8000 + j;

			if (moveId != aliasMoveId) {
				continue;
			}

			displayedMove->flags |= EditorMoveFlags_Generic;

			if (displayedMove->aliasId == 0)
			{
				displayedMove->color |= TEditorUtils::GetMoveColorFromFlag(displayedMove->flags);
				aliasId = aliasId;
			}
		}
	}

	displayedMove->moveId_str = std::to_string(moveId);
	displayedMove->name = moveName;
	displayedMove->alias_str = aliasId == 0 ? std::string() : std::to_string(aliasId);
	displayedMove->color = TEditorUtils::GetMoveColorFromFlag(flags);
	displayedMove->aliasId = aliasId;
	displayedMove->flags = flags;
}

void EditorT7::ReloadDisplayableMoveList()
{
	mustReloadMovelist = true;

	for (auto& move : displayableMovelist) {
		delete move;
	}
	displayableMovelist.clear();

	uint64_t movesetListOffset = m_offsets->movesetBlock + (uint64_t)m_infos->table.move;
	gAddr::Move* movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);
	char const* namePtr = (char const*)(m_movesetData + m_offsets->nameBlock);


	uint16_t moveId = 0;
	for (gAddr::Move* move = movePtr; moveId < m_infos->table.moveCount; ++moveId, ++move)
	{
		std::string moveName = std::string(namePtr + move->name_addr);

		EditorMoveFlags flags = 0;

		if (move->hitlevel || move->hitbox_location || move->first_active_frame || move->last_active_frame) {
			if (move->hitlevel && move->hitbox_location && move->first_active_frame && move->last_active_frame) {
				flags |= EditorMoveFlags_Attack;
			}
			else {
				flags |= EditorMoveFlags_OtherAttack;
			}
		}

		if (Helpers::endsWith<std::string>(moveName, "_y")) {
			flags |= EditorMoveFlags_Throw;
		}
		else if (Helpers::endsWith<std::string>(moveName, "_n")) {
			flags |= EditorMoveFlags_ThrowReaction;
		}

		if (Helpers::startsWith<std::string>(moveName, MOVESET_CUSTOM_MOVE_NAME_PREFIX)) {
			flags |= EditorMoveFlags_Custom;
		}

		displayableMovelist.push_back(new DisplayableMove{
			.moveId_str = std::to_string(moveId),
			.name = moveName,
			.alias_str = std::string(),
			.color = TEditorUtils::GetMoveColorFromFlag(flags),
			.moveId = moveId,
			.aliasId = 0,
			.flags = flags,
		});
	}

	// Mark default generic moves
	{
		uint16_t* aliases = m_infos->orig_aliases;
		uint8_t aliasesCount = (uint8_t)_countof(m_infos->orig_aliases);

		for (uint8_t j = 0; j < aliasesCount; ++j)
		{
			uint16_t moveId = aliases[j];
			uint16_t aliasId = 0x8000 + j;

			auto& move = displayableMovelist[moveId];
			move->flags |= EditorMoveFlags_Generic;

			if (move->aliasId == 0)
			{
				move->color |= TEditorUtils::GetMoveColorFromFlag(move->flags);
				move->aliasId = aliasId;
				move->alias_str = std::to_string(aliasId);
			}
		}
	}

	// Mark current generic moves
	{
		uint16_t* aliases = m_infos->current_aliases;
		uint8_t aliasesCount = (uint8_t)_countof(m_infos->current_aliases);

		for (uint8_t j = 0; j < aliasesCount; ++j)
		{
			uint16_t moveId = aliases[j];
			uint16_t aliasId = 0x8000 + j;

			auto& move = displayableMovelist[moveId];
			move->flags |= EditorMoveFlags_CurrentGeneric;

			if (move->aliasId == 0)
			{
				move->color |= TEditorUtils::GetMoveColorFromFlag(move->flags);
				move->aliasId = aliasId;
				move->alias_str = std::to_string(aliasId);
			}
		}
	}
}

// ===== Utils ===== //

uint16_t EditorT7::GetCurrentMoveID(uint8_t playerId)
{
	gameAddr playerAddress = m_game.ReadPtrPath("p1_addr");
	if (playerId > 0) {
		playerAddress += playerId * m_game.GetValue("playerstruct_size");
	}

	uint16_t moveId = m_process.readInt16(playerAddress + m_game.GetValue("currmove_id"));
	if (moveId >= 0x8000) {
		moveId = aliases[(size_t)(moveId - (uint16_t)0x8000)];
	}

	return moveId;
}

void EditorT7::SetCurrentMove(uint8_t playerId, gameAddr playerMoveset, size_t moveId)
{
	gameAddr playerAddress = m_game.ReadPtrPath("p1_addr");
	if (playerId > 0) {
		playerAddress += playerId * m_game.GetValue("playerstruct_size");
	}

	{
		gameAddr movesetOffset = playerAddress + m_game.GetValue("motbin_offset");
		// movesetOffset is the one offset we can't touch because it is the moveset the characte reverts to when transitioning to a generic anim
		// Since we want P2 to be able to play those moves here without changing their moveset, we don't write on it
		m_process.writeInt64(movesetOffset + 0x8, playerMoveset);
		m_process.writeInt64(movesetOffset + 0x10, playerMoveset);
		m_process.writeInt64(movesetOffset + 0x18, playerMoveset);
		m_process.writeInt64(movesetOffset + 0x20, playerMoveset);
	}

	if (moveId >= 0x8000) {
		// If is alias, convert it to its regular move id thanks to the alias list (uint16_t each) starting at 0x28
		moveId = m_process.readInt16(playerMoveset + 0x28 + (0x2 * (moveId - 0x8000)));
	}

	gameAddr moveAddr = m_process.readInt64(playerMoveset + 0x210) + moveId * sizeof(Move);

	// Write a big number to the frame timer to force the current move end
	m_process.writeInt32(playerAddress + m_game.GetValue("currmove_timer"), 99999);
	// Tell the game which move to play NEXT
	m_process.writeInt64(playerAddress + m_game.GetValue("nextmove_addr"), moveAddr);
	// Also tell the ID of the current move. This isn't required per se, but not doing that would make the current move ID 0, which i don't like.
	m_process.writeInt64(playerAddress + m_game.GetValue("currmove_id"), moveId);
}

void EditorT7::DeleteAnimationIfUnused(uint64_t anim_addr, uint64_t anim_name_addr)
{
	// Anim was changed, delete the old one if necessary

	bool animationStillInUse = false;
	bool animationNameStillInUse = false;

	for (auto& m : m_iterators.moves)
	{
		char* move_name = (char*)(m_movesetData + m_offsets->nameBlock + m.name_addr);
		char* anim_name = (char*)(m_movesetData + m_offsets->nameBlock + m.anim_name_addr);

		if (m.anim_addr == anim_addr) {
			animationStillInUse = true;
		}
		if (m.anim_name_addr == anim_name_addr) {
			animationNameStillInUse = true;
		}
	}

	if (!animationStillInUse) {
		// If the animation is unused then its name also is unused because we ensure every anim has a unique name when loading the moveset
		DeleteAnimation(anim_addr);
		DeleteNameBlockString(anim_name_addr);
	}
	else if (!animationNameStillInUse) {
		DeleteNameBlockString(anim_name_addr);
	}
	else {
		DEBUG_LOG("Anim still in use: not deleting it.\n");
	}
}

std::string EditorT7::ImportAnimation(const wchar_t* filepath, int moveid)
{
	// Keep file name only
	std::wstring animName_wstr = filepath;
	animName_wstr = animName_wstr.substr(animName_wstr.find_last_of(L"/\\") + 1);
	animName_wstr = animName_wstr.substr(0, animName_wstr.find_last_of(L'.'));
	std::string animName_str = Helpers::to_utf8(animName_wstr);

	Byte* anim;
	uint64_t animSize;
	uint64_t realAnimSize;
	{
		std::ifstream animFile(filepath, std::ios::binary);

		if (animFile.fail()) {
			DEBUG_ERR("Failed to read anim file '%S'", filepath);
			return "";
		}

		animFile.seekg(0, std::ios::end);
		animSize = animFile.tellg();
        try {
            anim = new Byte[animSize];
        } catch (std::bad_alloc) {
            DEBUG_ERR("Failed to allocate anim size %llun", animSize);
            return "";
        }
		animFile.seekg(0, std::ios::beg);
		animFile.read((char*)anim, animSize);

		// Ensure old bad animations don't get completely carried over
		realAnimSize = TAnimUtils::FromMemory::Safe::GetAnimSize(anim, animSize);

		if (realAnimSize == 0) {
			DEBUG_ERR("Bad animation file : %S", filepath);
			delete[] anim;
			return "";
		}

		if (realAnimSize < animSize) {
			animSize = realAnimSize;
			DEBUG_LOG("Imported animation is too big (useless bytes): shrinking its size\n");
		}
		else if (realAnimSize > animSize) {
			// todo: Display a warning to the user
			DEBUG_ERR("Imported animation is missing bytes. (file %llu, real %llu)", animSize, realAnimSize);
		}
	}

	// Byteswap the animation in little endian if needed
	if (!TAnimUtils::FromMemory::IsLittleEndian(anim)) {
		TAnimUtils::FromMemory::ByteswapAnimation(anim);
	}

	// Ensure animation name is unique
	if (m_animNameToOffsetMap.find(animName_str) != m_animNameToOffsetMap.end())
	{
		// An animation with the same name has been found
		auto anim_offset = m_animNameToOffsetMap[animName_str];
		Byte* existing_anim = (Byte*)(m_movesetData + m_offsets->animationBlock + (uint64_t)anim_offset);

		if (TAnimUtils::FromMemory::GetAnimSize(existing_anim) == realAnimSize && memcmp(anim, existing_anim, realAnimSize) == 0)
		{
			// todo: show popup
			DEBUG_LOG("(1) Attempted to import duplicate animation: using existing one.\n");
			auto old_anim_addr = m_iterators.moves[moveid]->anim_addr;
			auto old_anim_name_addr = m_iterators.moves[moveid]->anim_name_addr;

			m_iterators.moves[moveid]->anim_name_addr = m_animOffsetToNameOffset[anim_offset];
			m_iterators.moves[moveid]->anim_addr = anim_offset;

			DeleteAnimationIfUnused(old_anim_addr, old_anim_name_addr);
			delete[] anim;
			return animName_str;
		}


		std::string animName_orig = animName_str;
		animName_str += " (2)";
		unsigned int num = 2;
		while (m_animNameToOffsetMap.find(animName_str) != m_animNameToOffsetMap.end()) {
			animName_str = std::format("{} {}", animName_orig.c_str(), ++num);
		}
	}
	else
	{
		// Name is unique, but will still (quickly) try to assert if the animation exists under a different name
		Byte* anim_block = (Byte*)(m_movesetData + m_offsets->animationBlock);

		std::set<gameAddr> animOffsets;
		for (auto& move : m_iterators.moves) {
			animOffsets.insert(move.anim_addr);
		}

		auto offset_cursor = animOffsets.begin();
		auto offset_cursor_2 = animOffsets.begin();
		std::advance(offset_cursor_2, 1);
		while (offset_cursor_2 != animOffsets.end())
		{
			uint64_t size = (*offset_cursor_2) - (*offset_cursor);

			if (size == realAnimSize && memcmp(anim, anim_block + *offset_cursor, realAnimSize) == 0)
			{
				DEBUG_LOG("(2) Attempted to import duplicate animation: using existing one.\n");
				auto old_anim_addr = m_iterators.moves[moveid]->anim_addr;
				auto old_anim_name_addr = m_iterators.moves[moveid]->anim_name_addr;

				m_iterators.moves[moveid]->anim_name_addr = m_animOffsetToNameOffset[*offset_cursor];
				m_iterators.moves[moveid]->anim_addr = *offset_cursor;

				DeleteAnimationIfUnused(old_anim_addr, old_anim_name_addr);
				delete[] anim;
				return animName_str;
			}

			std::advance(offset_cursor, 1);
			std::advance(offset_cursor_2, 1);
		}
	}

	// Importation start
	const char* animName = animName_str.c_str();
	const size_t animNameSize = strlen(animName) + 1;

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	// Find position where to insert new name
	uint64_t moveNameOffset = m_header->moveset_data_start + m_offsets->movesetBlock;
	const uint64_t orig_nameBlockEnd = moveNameOffset;
	while (*(m_moveset + (moveNameOffset - 2)) == 0)
	{
		// Have to find the insert offset backward because the name block is always aligned to 8 bytes
		// We want to erase as many empty bytes for writing because of past alignment and then re-align to 8 bytes for the next bllock
		moveNameOffset--;
	}

	const uint64_t nameBlockEnd = Helpers::align8Bytes(moveNameOffset + animNameSize);
	const uint64_t orig_animBlockEnd = m_header->moveset_data_start + m_offsets->motaBlock;
	const uint64_t newAnimOffset = nameBlockEnd + m_offsets->motaBlock - m_offsets->movesetBlock;
	const uint64_t animBlockEnd = Helpers::align8Bytes(newAnimOffset + animSize);
	const uint64_t movesetAndAnimBlockSize = m_offsets->motaBlock - m_offsets->movesetBlock;

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = m_movesetSize + (animBlockEnd - orig_animBlockEnd);
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		// Return empty string to indicate error
		return std::string();
	}

	// Shift offsets in the moveset table & in our header
	const uint64_t extraNameSize = nameBlockEnd - orig_nameBlockEnd;
	const uint64_t extraAnimSize = Helpers::align8Bytes(animSize);

	for (unsigned int i = 0; i < m_header->block_list_size; ++i)
	{
		uint64_t blockOffset = (m_header->moveset_data_start + m_offsets->blocks[i]);
		if (blockOffset >= orig_animBlockEnd) {
			m_offsets->blocks[i] += extraNameSize + extraAnimSize;
		}
		else if (blockOffset >= orig_nameBlockEnd) {
			m_offsets->blocks[i] += extraNameSize;
		}
	}

	const uint64_t relativeName = moveNameOffset - m_offsets->nameBlock - m_header->moveset_data_start;
	const uint64_t relativeAnim = newAnimOffset - m_offsets->animationBlock - m_header->moveset_data_start;

	// Copy start //
	memcpy(newMoveset, m_moveset, moveNameOffset);

	// Write our new name
	memcpy(newMoveset + moveNameOffset, animName, animNameSize);

	// Copy all the data up to the animation (from name block end, which is moveset block start, to anim block end, which is mota block start)
	memcpy(newMoveset + nameBlockEnd, m_moveset + orig_nameBlockEnd, movesetAndAnimBlockSize);

	// Write our animation
	memcpy(newMoveset + newAnimOffset, anim, animSize);
	delete[] anim;

	// Copy all the data after the animation
	memcpy(newMoveset + animBlockEnd, m_moveset + orig_animBlockEnd, m_movesetSize - orig_animBlockEnd);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	auto old_anim_addr = m_iterators.moves[moveid]->anim_addr;
	auto old_anim_name_addr = m_iterators.moves[moveid]->anim_name_addr;

	m_iterators.moves[moveid]->anim_name_addr = relativeName;
	m_iterators.moves[moveid]->anim_addr = relativeAnim;

	DeleteAnimationIfUnused(old_anim_addr, old_anim_name_addr);

	m_animNameToOffsetMap[animName_str] = relativeAnim;
	m_animOffsetToNameOffset[relativeAnim] = relativeName;

	return animName_str;
}

uint32_t EditorT7::CalculateCRC32()
{
	// Get list of properties and its count
	TKMovesetProperty* customPropertyList = (TKMovesetProperty*)(m_moveset + Helpers::align8Bytes(m_header->header_size));
	uint64_t customPropertyCount = 0;

	while (customPropertyList[customPropertyCount].id != TKMovesetProperty_END) {
		++customPropertyCount;
	}

	++customPropertyCount;

	std::vector<std::pair<Byte*, uint64_t>> hashedFileBlocks{
		{(Byte*)customPropertyList, customPropertyCount * sizeof(TKMovesetProperty)},

		{m_movesetData + m_offsets->movesetInfoBlock, m_offsets->tableBlock - m_offsets->movesetInfoBlock },
		{m_movesetData + m_offsets->tableBlock, m_offsets->motalistsBlock - m_offsets->tableBlock },
		{m_movesetData + m_offsets->motalistsBlock, m_offsets->nameBlock - m_offsets->motalistsBlock },
		{m_movesetData + m_offsets->movesetBlock, m_offsets->animationBlock - m_offsets->movesetBlock },
		{m_movesetData + m_offsets->animationBlock, m_offsets->motaBlock - m_offsets->animationBlock },
		{m_movesetData + m_offsets->motaBlock, m_offsets->movelistBlock - m_offsets->motaBlock },
	};

	// Because the editor might make corrections to the moveset right as it loads it,
	// ... it is likely the CRC32 will be different from extraction
	// That isn't that big of a problem
	return Helpers::CalculateCrc32(hashedFileBlocks);
}

void EditorT7::SetSharedMemHandler(Online** sharedMemHandler)
{
	m_sharedMemHandler = (OnlineT7**)sharedMemHandler;
}

void EditorT7::ExecuteExtraprop(EditorInput* idField, EditorInput* valueField)
{
	if (m_sharedMemHandler && *m_sharedMemHandler && m_process.IsAttached())
	{
		if (!(*m_sharedMemHandler)->injectedDll) {
			if (!(*m_sharedMemHandler)->InjectDllAndWaitEnd()) {
				return;
			}
		}

		uint32_t id = (uint32_t)EditorUtils::GetFieldValue(idField);
		uint32_t value = (uint32_t)EditorUtils::GetFieldValue(valueField);
		(*m_sharedMemHandler)->ExecuteExtraprop(currentPlayerId, (uint32_t)id, (uint32_t)value);
	}
}