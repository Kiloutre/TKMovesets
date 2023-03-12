#include <map>

# include "Editor_t7.hpp"
# include "Helpers.hpp"

#define gAddr StructsT7_gameAddr

// ===== MOVES ===== //

std::map<std::string, EditorInput*> EditorT7::GetMoveInputs(uint16_t moveId, std::vector<EditorInput*>& drawOrder)
{

	std::map<std::string, EditorInput*> inputMap;

	// Set up fields
	inputMap["vulnerability"] = new EditorInput{
		.memberSize = 4,
		.category = 0,
		.imguiInputFlags = ImGuiInputTextFlags_CharsDecimal,
		.flags = EditorInputType_unsigned
	};

	inputMap["hitlevel"] = new EditorInput{
		.memberSize = 4,
		.category = 0,
		.imguiInputFlags = ImGuiInputTextFlags_CharsDecimal,
		.flags = EditorInputType_unsigned
	};

	inputMap["cancel_id"] = new EditorInput{
		.memberSize = 8,
		.category = 0,
		.imguiInputFlags = ImGuiInputTextFlags_CharsDecimal,
	};

	for (auto& [name, input] : inputMap) {
		// Duplicate the name inside the structure, this is more convenient for me
		input->name = name;
		input->field_fullname = "edition.move_field." + name;
	}

	// Set up draw order
	drawOrder.push_back(inputMap["vulnerability"]);
	drawOrder.push_back(inputMap["hitlevel"]);
	drawOrder.push_back(inputMap["cancel_id"]);

	// Set up default field values (read from moveset data
	uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
	gAddr::Move* move = (gAddr::Move*)(m_movesetData + movesetListOffset) + moveId;

	sprintf_s(inputMap["vulnerability"]->buffer, FORM_INPUT_BUFSIZE, "%u", move->vuln);
	sprintf_s(inputMap["hitlevel"]->buffer, FORM_INPUT_BUFSIZE, "%u", move->hitlevel);
	sprintf_s(inputMap["cancel_id"]->buffer, FORM_INPUT_BUFSIZE, "%lld", move->cancel_addr);
	
	return inputMap;
}


void EditorT7::SaveMove(uint16_t moveId, std::map<std::string, EditorInput*>& inputs)
{
	uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
	gAddr::Move* move = (gAddr::Move*)(m_movesetData + movesetListOffset) + moveId;

	move->vuln = (uint32_t)atoi(inputs["vulnerability"]->buffer);
	move->hitlevel = (uint32_t)atoi(inputs["hitlevel"]->buffer);
	move->cancel_addr = atoll(inputs["cancel_id"]->buffer);
}

bool EditorT7::ValidateMoveField(std::string name, EditorInput* field)
{
	if (name == "cancel_id" || name == "cancel_id_2" ||
		name == "cancel_id_3" || name == "cancel_id_4") {
		int listIdx = atoi(field->buffer);
		if (listIdx < -1 || listIdx >= m_infos->table.cancelCount) {
			return false;
		}
	}

	if (name == "extraproperty") {
		int listIdx = atoi(field->buffer);
		if (listIdx < -1 || listIdx >= m_infos->table.extraMovePropertyCount) {
			return false;
		}
	}

	if (name == "hitcondition") {
		int listIdx = atoi(field->buffer);
		if (listIdx < -1 || listIdx >= m_infos->table.hitConditionCount) {
			return false;
		}
	}

	if (name == "start_extraproperty") {
		int listIdx = atoi(field->buffer);
		if (listIdx < -1 || listIdx >= m_infos->table.moveBeginningPropCount) {
			return false;
		}
	}

	if (name == "end_extraproperty") {
		int listIdx = atoi(field->buffer);
		if (listIdx < -1 || listIdx >= m_infos->table.moveEndingPropCount) {
			return false;
		}
	}

	if (name == "voiceclip") {
		int listIdx = atoi(field->buffer);
		if (listIdx < -1 || listIdx >= m_infos->table.voiceclipCount) {
			return false;
		}
	}

	if (name == "transition") {
		uint16_t moveId = atoi(field->buffer);
		if (moveId >= m_infos->table.moveCount) {
			if (moveId < 0x8000) {
				return false;
			}
			if (moveId >= (0x8000 + m_aliases.size())) {
				return false;
			}
		}
	}
	return true;
}

// ===== Generic =====

bool EditorT7::ValidateField(std::string fieldType, std::string fieldShortName, EditorInput* field)
{
	if (field->memberSize == 4) {
		if (field->flags & EditorInputType_signed) {
			long long num = atoll(field->buffer);
			if (num > INT_MAX || num < INT_MIN) {
				return false;
			}
		}
		else if (field->flags & EditorInputType_unsigned) {
			long long num = atoll(field->buffer);
			if (num > UINT_MAX || num < 0) {
				return false;
			}
		}
	}

	if (field->memberSize == 2) {
		if (field->flags & EditorInputType_signed) {
			int num = atoi(field->buffer);
			if (num > SHRT_MAX || num < SHRT_MIN) {
				return false;
			}
		}
		else if (field->flags & EditorInputType_unsigned) {
			int num = atoi(field->buffer);
			if (num > USHRT_MAX || num < 0) {
				return false;
			}
		}
	}

	if (fieldType == "move") {
		return ValidateMoveField(fieldShortName, field);
	}

	return true;
}

// ===== Other ===== //

void EditorT7::LoadMoveset(Byte* t_moveset, uint64_t t_movesetSize)
{
	m_moveset = t_moveset;
	m_movesetSize = t_movesetSize;

	// Start getting pointers toward useful data structures
	// Also get the actual game-moveset (past our header) pointer
	m_header = (TKMovesetHeader*)t_moveset;
	m_movesetData = t_moveset + m_header->offsets.movesetInfoBlock + m_header->infos.header_size;
	m_infos = (MovesetInfo*)m_movesetData;

	// Get aliases as a vector
	uint16_t* aliasesPtr = m_infos->aliases1;
	for (size_t i = 0; i < 112 + 36; ++i) {
		m_aliases.push_back(aliasesPtr[i]);
	}
}

EditorTable EditorT7::GetMovesetTable()
{
	return EditorTable{
		.aliases = m_aliases,
	};
}

std::vector<DisplayableMove*> EditorT7::GetDisplayableMoveList()
{
	std::vector<DisplayableMove*> moves;

	uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
	gAddr::Move* movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);
	char const* namePtr = (char const*)(m_movesetData + m_header->offsets.nameBlock);

	uint16_t* aliases = m_infos->aliases1;
	uint8_t aliasesCount = (uint8_t)m_aliases.size();

	uint16_t moveId = 0;
	for (gAddr::Move* move = movePtr; moveId < m_infos->table.moveCount; ++moveId, ++move)
	{
		std::string moveName = std::string(namePtr + move->name_addr);
		uint16_t aliasId = 0;
		EditorMoveFlags flags = 0;

		for (uint8_t j = 0; j < aliasesCount; ++j) {
			if (aliases[j] == moveId) {
				aliasId = 0x8000 + j;
				flags |= EditorMoveFlags_Generic;
				break;
			}
		}

		if (move->hitlevel || move->hitbox_location || move->first_active_frame || move->last_active_frame) {
			if (move->hitlevel && move->hitbox_location && move->first_active_frame && move->last_active_frame) {
				flags |= EditorMoveFlags_Attack;
			} else {
				flags |= EditorMoveFlags_OtherAttack;
			}
		}

		if (Helpers::endsWith(moveName, "_y")) {
			flags |= EditorMoveFlags_Throw;
		} else if (Helpers::endsWith(moveName, "_n")) {
			flags |= EditorMoveFlags_ThrowReaction;
		}

		if (Helpers::startsWith(moveName, "tkm_")) {
			flags |= EditorMoveFlags_Custom;
		}

		moves.push_back(new DisplayableMove{
			.moveId = moveId,
			.name = moveName,
			.aliasId = aliasId,
			.flags = flags,
			.color = EditorUtils::GetMoveColorFromFlag(flags)
		});
	}

	return moves;
}

// ===== Utils ===== //

uint16_t EditorT7::GetCurrentMoveID(uint8_t playerId)
{
	gameAddr playerAddress = m_game->ReadPtr("t7_p1_addr");
	if (playerId > 0) {
		playerAddress += playerId * m_game->addrFile->GetSingleValue("val:t7_playerstruct_size");
	}

	uint16_t moveId = m_process->readInt16(playerAddress + m_game->addrFile->GetSingleValue("val:t7_currmove_id"));
	if (moveId >= 0x8000) {
		moveId = m_aliases[moveId - 0x8000];
	}

	return moveId;
}


void EditorT7::SetCurrentMove(uint8_t playerId, gameAddr playerMoveset, size_t moveId)
{
	gameAddr playerAddress = m_game->ReadPtr("t7_p1_addr");
	if (playerId > 0) {
		playerAddress += playerId * m_game->addrFile->GetSingleValue("val:t7_playerstruct_size");
	}

	{
		gameAddr movesetOffset = playerAddress + m_game->addrFile->GetSingleValue("val:t7_motbin_offset");
		// movesetOffset is the one offset we can't touch because it is the moveset the characte reverts to when transitioning to a generic anim
		m_process->writeInt64(movesetOffset + 0x8, playerMoveset);
		m_process->writeInt64(movesetOffset + 0x10, playerMoveset);
		m_process->writeInt64(movesetOffset + 0x18, playerMoveset);
		m_process->writeInt64(movesetOffset + 0x20, playerMoveset);
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