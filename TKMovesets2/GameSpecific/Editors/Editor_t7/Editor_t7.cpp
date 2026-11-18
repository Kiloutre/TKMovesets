#include "Importer_t7/Importer_t7_Utils.hpp"
#include <map>
#include <format>
#include <fstream>

#include "Compression.hpp"
#include "helpers.hpp"
#include "Importer.hpp"
#include "Editor_t7.hpp"
#include "Animations.hpp"
#include "RetailT8AnimToT7.hpp"

using namespace StructsT7;
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
    if (!moveName) throw std::invalid_argument("Missing name");
    const std::string name(moveName);
    const auto offset = m_offsets->movesetBlock - m_offsets->nameBlock;
    ReplaceBlockRange(StructsT7::TKMovesetHeaderBlocks_Name, offset, 0,
        {reinterpret_cast<const Byte*>(name.c_str()), name.size() + 1});
    return offset;
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

	if (!CompressionUtils::RAW::Moveset::ExpandFileBuffer(t_moveset, t_movesetSize)) {
		free(t_moveset);
		return false;
	}
	auto header = reinterpret_cast<const TKMovesetHeader*>(t_moveset);
	auto offsets = reinterpret_cast<const TKMovesetHeaderBlocks*>(t_moveset + header->block_list);
	if (header->block_list_size < std::size(offsets->blocks) ||
		!T7ImportUtils::ValidateT7Semantics(*offsets, t_moveset + header->moveset_data_start, t_movesetSize - header->moveset_data_start)) {
		free(t_moveset);
		return false;
	}

	LoadMovesetPtr(t_moveset, t_movesetSize);

	if ((m_header->flags & MovesetFlags_MovesetModified) == 0) {
		// First time this moveset is edited
		strncat_s(m_header->origin, sizeof(m_header->origin), " (M)", _TRUNCATE); // a full origin field is left as is
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
			{
				// Reallocation was done, update the pointers we are using
				movesetListOffset = m_offsets->movesetBlock + (uint64_t)m_infos->table.move;
				movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);
				namePtr = (char const*)(m_movesetData + m_offsets->nameBlock);
				movePtr[i].anim_name_addr = newNameOffset;
				animName = namePtr + movePtr[i].anim_name_addr;
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

	// Same flags, colour and alias as ReloadDisplayableMoveList
	uint16_t aliasId = 0;
	unsigned int color = TEditorUtils::GetMoveColorFromFlag(flags);
	auto displayedMove = displayableMovelist[moveId];

	// Mark if default generic moves, then if current generic move
	const std::pair<uint16_t*, EditorMoveFlags> aliasLists[] = {
		{ m_infos->orig_aliases, EditorMoveFlags_Generic },
		{ m_infos->current_aliases, EditorMoveFlags_CurrentGeneric },
	};
	static_assert(_countof(m_infos->orig_aliases) == _countof(m_infos->current_aliases));
	for (const auto& [aliases, aliasFlag] : aliasLists)
	{
		for (uint8_t j = 0; j < (uint8_t)_countof(m_infos->orig_aliases); ++j)
		{
			if (moveId != aliases[j]) {
				continue;
			}

			flags |= aliasFlag;

			if (aliasId == 0)
			{
				color |= TEditorUtils::GetMoveColorFromFlag(flags);
				aliasId = 0x8000 + j;
			}
		}
	}

	displayedMove->moveId_str = std::to_string(moveId);
	displayedMove->name = moveName;
	displayedMove->alias_str = aliasId == 0 ? std::string() : std::to_string(aliasId);
	displayedMove->color = color;
	displayedMove->aliasId = aliasId;
	displayedMove->flags = flags;
	displayedMove->screw = MoveUsesScrew(moveId);
	displayedMove->rage = MoveSpendsRage(moveId);
}

void EditorT7::ReloadDisplayableMoveList()
{
    if (m_infos->table.moveCount > uint64_t(UINT16_MAX) + 1)
        throw std::length_error("Move IDs exceed the editor's 16-bit range");
    std::vector<DisplayableMove*> newRows;
    std::vector<std::unique_ptr<DisplayableMove>> ownedRows;
    newRows.reserve(m_infos->table.moveCount);
    ownedRows.reserve(m_infos->table.moveCount);

	uint64_t movesetListOffset = m_offsets->movesetBlock + (uint64_t)m_infos->table.move;
	gAddr::Move* movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);


	size_t moveId = 0;
	for (gAddr::Move* move = movePtr; moveId < m_infos->table.moveCount; ++moveId, ++move)
	{
		std::string moveName(BlockString(StructsT7::TKMovesetHeaderBlocks_Name, move->name_addr));

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

		ownedRows.emplace_back(std::make_unique<DisplayableMove>(DisplayableMove{
			.moveId_str = std::to_string(moveId),
			.name = moveName,
			.alias_str = std::string(),
			.color = TEditorUtils::GetMoveColorFromFlag(flags),
			.moveId = static_cast<uint16_t>(moveId),
			.aliasId = 0,
			.flags = flags,
			.screw = MoveUsesScrew(static_cast<uint16_t>(moveId)),
			.rage = MoveSpendsRage(static_cast<uint16_t>(moveId)),
		}));
        newRows.push_back(ownedRows.back().get());
	}

	// Mark default generic moves
	{
		uint16_t* aliases = m_infos->orig_aliases;
		uint8_t aliasesCount = (uint8_t)_countof(m_infos->orig_aliases);

		for (uint8_t j = 0; j < aliasesCount; ++j)
		{
			uint16_t moveId = aliases[j];
			uint16_t aliasId = 0x8000 + j;

			if (moveId >= newRows.size()) continue;
            auto& move = newRows[moveId];
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

			if (moveId >= newRows.size()) continue;
            auto& move = newRows[moveId];
			move->flags |= EditorMoveFlags_CurrentGeneric;

			if (move->aliasId == 0)
			{
				move->color |= TEditorUtils::GetMoveColorFromFlag(move->flags);
				move->aliasId = aliasId;
				move->alias_str = std::to_string(aliasId);
			}
		}
	}
    newRows.swap(displayableMovelist);
    for (auto& row : ownedRows) row.release();
    for (auto row : newRows) delete row;
    mustReloadMovelist = true;
}

// ===== Utils ===== //

uint16_t EditorT7::GetCurrentMoveID(uint8_t playerId) const
{
	gameAddr playerAddress = m_game.ReadPtrPath("p1_addr");
	if (playerId > 0) {
		playerAddress += playerId * m_game.GetValue("playerstruct_size");
	}

	uint16_t moveId = m_process.readInt16(playerAddress + m_game.GetValue("currmove_id"));
	if (moveId >= 0x8000) {
        const auto alias = static_cast<size_t>(moveId - 0x8000);
        if (alias >= aliases.size()) return 0;
        moveId = aliases[alias];
	}

	return moveId < m_infos->table.moveCount ? moveId : 0;
}

void EditorT7::SetCurrentMove(uint8_t playerId, gameAddr playerMoveset, size_t moveId)
{
    if (playerId > 1 || moveId > UINT16_MAX) throw std::out_of_range("Invalid player or move ID");
    GameProcess::WriteBatch transaction(m_process);
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
        if (moveId - 0x8000 >= std::size(m_infos->current_aliases)) throw std::out_of_range("Invalid move alias");
        moveId = m_process.readUInt16(playerMoveset + offsetof(MovesetInfo, current_aliases) + 2 * (moveId - 0x8000));
	}

    const auto tableAddress = playerMoveset + offsetof(MovesetInfo, table);
    const auto count = m_process.readUInt64(tableAddress + offsetof(MovesetTable, moveCount));
    if (moveId >= count) throw std::out_of_range("Move does not exist in the imported moveset");
    gameAddr moveAddr = m_process.readUInt64(tableAddress + offsetof(MovesetTable, move)) + moveId * sizeof(Move);

	// Write a big number to the frame timer to force the current move end
	m_process.writeInt32(playerAddress + m_game.GetValue("currmove_timer"), 99999);
	// Tell the game which move to play NEXT
	m_process.writeInt64(playerAddress + m_game.GetValue("nextmove_addr"), moveAddr);
	// Also tell the ID of the current move. This isn't required per se, but not doing that would make the current move ID 0, which i don't like.
    m_process.writeUInt16(playerAddress + m_game.GetValue("currmove_id"), static_cast<uint16_t>(moveId));
    const auto result = transaction.Commit();
    if (!result.ok) {
        live_loadedMoveset = 0;
        throw std::runtime_error(result.rollbackComplete ? "Could not play move; previous game values were restored." :
            "Could not play move; rollback was incomplete. Re-import before live editing.");
    }
}

std::vector<Byte> EditorT7::MoveAnimation(uint16_t moveId)
{
	if (moveId >= m_iterators.moves.size()) throw std::out_of_range("Move outside the move list");
	const uint64_t offset = m_iterators.moves[moveId]->anim_addr;
	const uint64_t blockSize = m_offsets->motaBlock - m_offsets->animationBlock;
	if (offset >= blockSize) throw std::out_of_range("Animation offset outside its section");
	const Byte* animation = m_movesetData + m_offsets->animationBlock + offset;
	const uint64_t size = TAnimUtils::FromMemory::Safe::GetAnimSize(animation, blockSize - offset);
	if (!size || size > blockSize - offset) throw std::runtime_error("Cannot read the move's animation");
	return std::vector<Byte>(animation, animation + size);
}

uint64_t EditorT7::MoveAnimationToken(uint16_t moveId)
{
	if (moveId >= m_iterators.moves.size()) return 0;
	return (static_cast<uint64_t>(m_iterators.moves[moveId]->anim_addr) << 1) | 1;
}

std::vector<AnimPreview::Displacement> EditorT7::MoveDisplacements(uint16_t moveId)
{
	std::vector<AnimPreview::Displacement> result;
	if (moveId >= m_iterators.moves.size()) return result;
	const uint64_t first = m_iterators.moves[moveId]->extra_move_property_addr;
	const uint64_t count = m_iterators.extra_move_properties.size();
	if (first == MOVESET_ADDR_MISSING || first >= count) return result;
	for (uint64_t row = first; row < count; ++row) {
		const auto* property = m_iterators.extra_move_properties[row];
		if (property->id == 0) break;
		if (auto kind = AnimPreview::T7DisplacementKind(property->id))
			result.push_back({AnimPreview::DisplacementFrame(property->starting_frame), *kind,
				AnimPreview::DisplacementValue(*kind, property->value_unsigned, false), property->id});
	}
	return result;
}

bool EditorT7::MoveUsesScrew(uint16_t moveId) const
{
	if (moveId >= m_iterators.moves.size()) return false;
	const uint64_t first = m_iterators.moves[moveId]->hit_condition_addr;
	if (first == MOVESET_ADDR_MISSING) return false;
	const uint32_t end = constants.at(EditorConstants_RequirementEnd);
	for (uint64_t row = first; row < m_iterators.hit_conditions.size() && row < first + 64; ++row) {
		const uint64_t list = m_iterators.hit_conditions[row]->requirements_addr;
		bool isDefault = true;
		for (uint64_t q = list; q < m_iterators.requirements.size() && q < list + 64; ++q) {
			const uint32_t condition = m_iterators.requirements[q]->condition;
			if (condition == end) break;
			isDefault = false;
			if (condition == 615) return true;
		}
		if (isDefault) break; // the list's default entry ends it
	}
	return false;
}

bool EditorT7::MoveSpendsRage(uint16_t moveId) const
{
	constexpr uint32_t SpendRage = 0x8272;
	if (moveId >= m_iterators.moves.size()) return false;
	const auto* move = m_iterators.moves[moveId];
	// Extra properties end at id 0
	const uint64_t extraCount = m_iterators.extra_move_properties.size();
	if (const uint64_t first = move->extra_move_property_addr; first != MOVESET_ADDR_MISSING)
		for (uint64_t row = first; row < extraCount && row < first + 512; ++row) {
			const auto* property = m_iterators.extra_move_properties[row];
			if (property->id == 0) break;
			if (property->id == SpendRage && property->value_unsigned != 0) return true;
		}
	// Start and end properties end at the requirement terminator (not scanned while the editor has no constants)
	const auto terminator = constants.find(EditorConstants_RequirementEnd);
	if (terminator == constants.end()) return false;
	const uint32_t end = terminator->second;
	auto scan = [&](uint64_t first, const auto& list) {
		if (first == MOVESET_ADDR_MISSING) return false;
		for (uint64_t row = first; row < list.size() && row < first + 512; ++row) {
			const auto* property = list[row];
			if (property->extraprop == end) break;
			if (property->extraprop == SpendRage && property->value != 0) return true;
		}
		return false;
	};
	return scan(move->move_start_extraprop_addr, m_iterators.move_start_properties) || scan(move->move_end_extraprop_addr, m_iterators.move_end_properties);
}

int32_t EditorT7::ResolveMoveId(uint32_t moveId) const
{
	if (moveId >= 0x8000) {
		if (moveId - 0x8000 >= aliases.size()) return -1;
		moveId = aliases[moveId - 0x8000];
	}
	return moveId < m_iterators.moves.size() ? static_cast<int32_t>(moveId) : -1;
}

AnimPreview::ReactionMoves EditorT7::MoveHitReactions(uint16_t moveId, bool opponentAirborne)
{
	AnimPreview::ReactionMoves result;
	if (moveId >= m_iterators.moves.size()) return result;
	const uint64_t first = m_iterators.moves[moveId]->hit_condition_addr;
	const uint64_t count = m_iterators.hit_conditions.size();
	if (first == MOVESET_ADDR_MISSING || first >= count) return result;
	const uint32_t requirementEnd = constants[EditorConstants_RequirementEnd];
	// Requirements judged for the preview's opponent: 0 (always), 607 opponent airborne, 608 not airborne, 614 in a
	// juggle. Any other requirement counts as unmet.
	auto met = [&](uint32_t condition) {
		switch (condition) {
		case 0: return true;
		case 607: case 614: return opponentAirborne;
		case 608: return !opponentAirborne;
		default: return false;
		}
	};
	uint64_t chosen = first;
	// The game takes the first entry whose requirements all hold; the list ends with its default entry (none), so a
	// list that never matches keeps its first entry.
	for (uint64_t row = first; row < count && row < first + 64; ++row) {
		bool all = true, isDefault = false;
		for (uint64_t q = m_iterators.hit_conditions[row]->requirements_addr; q < m_iterators.requirements.size(); ++q) {
			const uint32_t condition = m_iterators.requirements[q]->condition;
			if (condition == requirementEnd) { isDefault = q == m_iterators.hit_conditions[row]->requirements_addr; break; }
			if (!met(condition)) { all = false; break; }
		}
		if (all) { chosen = row; break; }
		if (isDefault) break;
	}
	result.damage = static_cast<int>(m_iterators.hit_conditions[chosen]->damage);
	const uint64_t reactions = m_iterators.hit_conditions[chosen]->reactions_addr;
	if (reactions >= m_iterators.reactions.size()) return result;
	const auto* reaction = m_iterators.reactions[reactions];
	// The game's move ids start at 0x50 (as in retail T8): the field named default_moveid holds the standing
	// reaction, and the one named standing_moveid (0x4E) is 0 in the game's movesets. There is no airborne slot.
	result.standing = ResolveMoveId(reaction->default_moveid);
	result.counterhit = ResolveMoveId(reaction->counterhit_moveid);
	result.leftSide = ResolveMoveId(reaction->left_side_moveid);
	result.rightSide = ResolveMoveId(reaction->right_side_moveid);
	result.backturned = ResolveMoveId(reaction->backturned_moveid);
	auto pushback = [&](uint64_t index) {
		AnimPreview::Pushback out;
		if (index >= m_iterators.pushbacks.size()) return out;
		const auto* p = m_iterators.pushbacks[index];
		out.duration = p->duration;
		out.displacement = p->displacement;
		for (uint64_t i = 0; i < p->num_of_loops && i < 256 && p->extradata_addr + i < m_iterators.pushback_extras.size(); ++i)
			out.offsets.push_back(m_iterators.pushback_extras[p->extradata_addr + i]->horizontal_offset);
		return out;
	};
	result.frontPushback = pushback(reaction->front_pushback);
	result.counterhitPushback = pushback(reaction->front_counterhit_pushback);
	result.leftPushback = pushback(reaction->left_side_pushback);
	result.rightPushback = pushback(reaction->right_side_pushback);
	result.backPushback = pushback(reaction->backturned_pushback);
	return result;
}

void EditorT7::DeleteAnimationIfUnused(uint64_t animation, uint64_t name)
{
    MutateBatch([&] {
        bool inUse = false;
        for (const auto& move : m_iterators.moves) if (move.anim_addr == animation) inUse = true;
        if (!inUse) DeleteAnimation(animation);
        const auto size = BlockString(TKMovesetHeaderBlocks_Name, name).size() + 1;
        if (!NameRangeReferenced(name, size)) DeleteNameBlockString(name);
    });
}

std::string EditorT7::ImportAnimation(const wchar_t* filepath, int moveid)
{
    // Keep the GUI's empty-string failure contract, with all failures outside the transaction.
    try {
        if (!filepath || moveid < 0 || static_cast<uint64_t>(moveid) >= m_iterators.moves.size()) return {};
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        const auto size = file.tellg();
        if (!file || size < 8 || size > (256LL << 20)) return {};
        std::vector<Byte> animation(static_cast<size_t>(size));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(animation.data()), animation.size())) return {};
        // Retail T8 PANM (raw/static tracks): converted to a T7 0xC8 body animation (RetailT8AnimToT7).
        if (!memcmp(animation.data() + 4, "PANM", 4))
            animation = RetailT8AnimToT7::Convert(animation, RetailT8AnimToT7::Format::Frames);
        const auto actual = TAnimUtils::FromMemory::Safe::GetAnimSize(animation.data(), animation.size());
        if (!actual) return {};
        animation.resize(static_cast<size_t>(actual));
        if (!TAnimUtils::FromMemory::IsLittleEndian(animation.data()) &&
            !TAnimUtils::FromMemory::ByteswapAnimation(animation.data(), animation.size())) return {};
        std::wstring filename(filepath);
        filename = filename.substr(filename.find_last_of(L"/\\") + 1);
        filename = filename.substr(0, filename.find_last_of(L'.'));
        auto requestedName = Helpers::to_utf8(filename);
        if (requestedName.empty()) requestedName = "animation";

        return MutateBatch([&]() -> std::string {
            RebuildAnimationMaps();
            const auto oldAnimation = m_iterators.moves[moveid]->anim_addr;
            const auto oldName = m_iterators.moves[moveid]->anim_name_addr;
            gameAddr target = UINT64_MAX, targetName = 0;
            std::string canonicalName;
            // Include the final/only animation and compare real bytes, ignoring alignment padding.
            const auto blockSize = m_offsets->motaBlock - m_offsets->animationBlock;
            for (const auto& [offset, name] : m_animOffsetToNameOffset) {
                if (offset >= blockSize) throw std::out_of_range("Animation offset outside its section");
                auto next = m_animOffsetToNameOffset.upper_bound(offset);
                const auto end = next == m_animOffsetToNameOffset.end() ? blockSize : next->first;
                auto existing = m_movesetData + m_offsets->animationBlock + offset;
                if (TAnimUtils::FromMemory::Safe::GetAnimSize(existing, end - offset) == animation.size() &&
                    memcmp(existing, animation.data(), animation.size()) == 0) {
                    target = offset;
                    targetName = name;
                    canonicalName = BlockString(TKMovesetHeaderBlocks_Name, name);
                    break;
                }
            }
            if (target == UINT64_MAX) {
                canonicalName = requestedName;
                unsigned suffix = 2;
                while (m_animNameToOffsetMap.contains(canonicalName))
                    canonicalName = std::format("{} ({})", requestedName, suffix++);
                targetName = CreateMoveName(canonicalName.c_str());
                target = m_offsets->motaBlock - m_offsets->animationBlock;
                ReplaceBlockRange(TKMovesetHeaderBlocks_Animation, target, 0, animation);
            }
            m_iterators.moves[moveid]->anim_addr = target;
            m_iterators.moves[moveid]->anim_name_addr = targetName;
            DeleteAnimationIfUnused(oldAnimation, oldName);
            RebuildAnimationMaps();
            return canonicalName;
        });
    } catch (const std::exception& error) {
        DEBUG_ERR("Animation import failed: %s", error.what());
        return {};
    }
}

uint32_t EditorT7::CalculateCRC32()
{
	// The checksum identifies the moveset data: like the exporter (T7ExportSnapshot) and the converters
	// (MovesetFile), it covers an empty property list whatever import profile the file carries.
	TKMovesetProperty end{ .id = TKMovesetProperty_END, .value = 0 };

	std::vector<std::pair<Byte*, uint64_t>> hashedFileBlocks{
		{reinterpret_cast<Byte*>(&end), sizeof(end)},

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
void EditorT7::RebuildAnimationMaps()
{
    decltype(m_animNameToOffsetMap) names;
    decltype(m_animOffsetToNameOffset) offsets;
    const auto blockSize = m_offsets->motaBlock - m_offsets->animationBlock;
    for (const auto& move : m_iterators.moves) {
        if (move.anim_addr >= blockSize) throw std::out_of_range("Animation offset outside its section");
        const std::string name(BlockString(TKMovesetHeaderBlocks_Name, move.anim_name_addr));
        auto [entry, inserted] = names.emplace(name, move.anim_addr);
        if (!inserted && entry->second != move.anim_addr) throw std::invalid_argument("Ambiguous animation name");
        offsets.try_emplace(move.anim_addr, move.anim_name_addr);
    }
    names.swap(m_animNameToOffsetMap);
    offsets.swap(m_animOffsetToNameOffset);
}

bool EditorT7::NameRangeReferenced(uint64_t offset, uint64_t size) const
{
    auto overlaps = [&](uint64_t ref) {
        const auto length = BlockString(TKMovesetHeaderBlocks_Name, ref).size() + 1;
        return ref < offset + size && offset < ref + length;
    };
    for (const auto& move : m_iterators.moves)
        if (overlaps(move.name_addr) || overlaps(move.anim_name_addr)) return true;
    for (auto ref : {m_infos->character_name_addr, m_infos->character_creator_addr, m_infos->date_addr, m_infos->fulldate_addr})
        if (auto value = reinterpret_cast<uint64_t>(ref);
            value >= m_offsets->nameBlock && value < m_offsets->movesetBlock && overlaps(value - m_offsets->nameBlock)) return true;
    return false;
}
