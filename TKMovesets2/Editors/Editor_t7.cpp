#include <map>

# include "Editor_t7.hpp"
# include "Helpers.hpp"

using namespace EditorUtils;

#define gAddr StructsT7_gameAddr
#define SET_DEFAULT_VAL(fieldName, format, value) sprintf_s(inputMap[fieldName]->buffer, FORM_INPUT_BUFSIZE, format, value)
#define CREATE_STRING_FIELD(a, c, g) drawOrder.push_back(a), inputMap[a] = new EditorInput { .category = c, .imguiInputFlags = 0, .flags = EditorInput_String }, SET_DEFAULT_VAL(a, "%s", g)
#define CREATE_FIELD(a, c, e, g) drawOrder.push_back(a), inputMap[a] = new EditorInput { .category = c, .imguiInputFlags = GetFieldCharset(e), .flags = e }, SET_DEFAULT_VAL(a, GetFieldFormat(e), g)


static void WriteFieldFullname(std::map<std::string, EditorInput*>& inputMap, std::string baseIdentifier)
{
	// Finishing touch
	for (auto& [name, input] : inputMap) {
		// Duplicate the name inside the structure, this is more convenient for me in some places, helps writing a lot shorter code
		input->name = name;
		input->field_fullname = baseIdentifier + name;
	}
}

// ===== ExtraProperties ===== //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetExtrapropListInputs(uint16_t extrapropId, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.extraMoveProperty;
	ExtraMoveProperty* prop = (ExtraMoveProperty*)(m_movesetData + movesetListOffset) + extrapropId;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	uint32_t idx = 0;
	do
	{
		std::map<std::string, EditorInput*> inputMap;

		CREATE_FIELD("starting_frame", 0, EditorInput_U32, prop->starting_frame);
		CREATE_FIELD("id", 0, EditorInput_H32, prop->id);
		CREATE_FIELD("param", 0, EditorInput_U32, prop->param);

		WriteFieldFullname(inputMap, "edition.extraproperty_field.");
		inputListMap.push_back(inputMap);
		++idx;
	} while ((prop++)->starting_frame != 0);

	return inputListMap;
}

bool EditorT7::ValidateExtrapropField(EditorInput* field)
{
	//
	return true;
}


void EditorT7::SaveExtrapropList(uint16_t voiceclipId, std::vector<std::map<std::string, EditorInput*>>& inputsList)
{
	/*
	uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.voiceclip;
	Voiceclip* voiceclip = (Voiceclip*)(m_movesetData + movesetListOffset) + voiceclipId;

	voiceclip->id = (uint32_t)strtol(inputs["id"]->buffer, nullptr, 16);
	*/
}
// ===== Voiceclips ===== //

std::map<std::string, EditorInput*> EditorT7::GetVoiceclipInputs(uint16_t voiceclipId, VectorSet<std::string>& drawOrder)
{
	std::map<std::string, EditorInput*> inputMap;

	uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.voiceclip;
	Voiceclip* voiceclip = (Voiceclip*)(m_movesetData + movesetListOffset) + voiceclipId;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_FIELD("id", 0, EditorInput_H32, voiceclip->id);

	WriteFieldFullname(inputMap, "edition.voiceclip_field.");
	return inputMap;
}

bool EditorT7::ValidateVoiceclipField(EditorInput* field)
{
	//
	return true;
}


void EditorT7::SaveVoiceclip(uint16_t voiceclipId, std::map<std::string, EditorInput*>& inputs)
{
	uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.voiceclip;
	Voiceclip* voiceclip = (Voiceclip*)(m_movesetData + movesetListOffset) + voiceclipId;

	voiceclip->id = (uint32_t)strtol(inputs["id"]->buffer, nullptr, 16);
}

// ===== MOVES ===== //

std::map<std::string, EditorInput*> EditorT7::GetMoveInputs(uint16_t moveId, VectorSet<std::string>& drawOrder)
{
	std::map<std::string, EditorInput*> inputMap;

	uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
	gAddr::Move* move = (gAddr::Move*)(m_movesetData + movesetListOffset) + moveId;

	char* nameBlock = (char*)(m_movesetData + m_header->offsets.nameBlock);

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_STRING_FIELD("move_name", 0, nameBlock + move->name_addr);
	CREATE_STRING_FIELD("anim_name", 0, nameBlock + move->anim_name_addr);
	CREATE_FIELD("vulnerability", 0, EditorInput_U32, move->vuln);
	CREATE_FIELD("hitlevel", 0, EditorInput_H32, move->hitlevel);
	CREATE_FIELD("transition", 0, EditorInput_U16 | EditorInput_Clickable, move->transition);
	CREATE_FIELD("moveId_val1", 0, EditorInput_U16, move->moveId_val1);
	CREATE_FIELD("moveId_val2", 0, EditorInput_U16, move->moveId_val2);
	CREATE_FIELD("anim_len", 0, EditorInput_U32, move->anim_len);
	CREATE_FIELD("airborne_start", 0, EditorInput_U32, move->airborne_start);
	CREATE_FIELD("airborne_end", 0, EditorInput_U32, move->airborne_end);
	CREATE_FIELD("ground_fall", 0, EditorInput_U32, move->ground_fall);
	CREATE_FIELD("hitbox_location", 0 , EditorInput_H32, move->hitbox_location);
	CREATE_FIELD("first_active_frame", 0, EditorInput_U32, move->first_active_frame);
	CREATE_FIELD("last_active_frame", 0, EditorInput_U32, move->last_active_frame);
	CREATE_FIELD("distance", 0, EditorInput_U16, move->distance);

	CREATE_FIELD("cancel_id", 2, EditorInput_PTR,  move->cancel_addr);
	CREATE_FIELD("hit_condition_id", 2, EditorInput_PTR, move->hit_condition_addr);
	CREATE_FIELD("extra_properties_id", 2, EditorInput_PTR, move->extra_move_property_addr);
	CREATE_FIELD("beginning_extra_properties_id", 2, EditorInput_PTR, move->move_start_extraprop_addr);
	CREATE_FIELD("ending_extra_properties_id", 2, EditorInput_PTR, move->move_end_extraprop_addr);
	CREATE_FIELD("voiceclip_id", 2, EditorInput_PTR, move->voicelip_addr);

	CREATE_FIELD("cancel_id_2", 3, EditorInput_PTR, move->_0x28_cancel_addr);
	CREATE_FIELD("cancel_id_2_related", 3, EditorInput_U32, move->_0x30_int__0x28_related);
	CREATE_FIELD("cancel_id_3", 3, EditorInput_PTR, move->_0x38_cancel_addr);
	CREATE_FIELD("cancel_id_3_related", 3, EditorInput_U32, move->_0x40_int__0x38_related);
	CREATE_FIELD("cancel_id_4", 3, EditorInput_PTR, move->_0x48_cancel_addr);
	CREATE_FIELD("cancel_id_4_related", 3, EditorInput_U32, move->_0x50_int__0x48_related);

	CREATE_FIELD("_0x34_int", 5, EditorInput_U32, move->_0x34_int);
	CREATE_FIELD("_0x44_int", 5, EditorInput_U32, move->_0x44_int);
	CREATE_FIELD("_0x56_short", 5, EditorInput_U16, move->_0x56_short);
	CREATE_FIELD("_0x5C_short", 5, EditorInput_U16, move->_0x5C_short);
	CREATE_FIELD("_0x5E_short", 5, EditorInput_U16, move->_0x5E_short);
	CREATE_FIELD("_0x98_int", 5, EditorInput_U32, move->_0x98_int);
	CREATE_FIELD("_0xA8_short", 5, EditorInput_U16, move->_0xA8_short);
	CREATE_FIELD("_0xAC_short", 5, EditorInput_U16, move->_0xAC_short);

	WriteFieldFullname(inputMap, "edition.move_field.");
	return inputMap;
}

void EditorT7::SaveMove(uint16_t moveId, std::map<std::string, EditorInput*>& inputs)
{
	uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
	gAddr::Move* move = (gAddr::Move*)(m_movesetData + movesetListOffset) + moveId;

	// todo: move name, allow edition
	if (m_animNameMap.find(inputs["anim_name"]->buffer) != m_animNameMap.end()) {
		move->anim_addr = m_animNameMap[inputs["anim_name"]->buffer];
	}
	move->vuln = (uint32_t)atoi(inputs["vulnerability"]->buffer);
	move->hitlevel = (uint32_t)strtol(inputs["hitlevel"]->buffer, nullptr, 16);
	move->transition = (uint16_t)atoi(inputs["transition"]->buffer);
	move->moveId_val1 = (uint16_t)atoi(inputs["moveId_val1"]->buffer);
	move->moveId_val2 = (uint16_t)atoi(inputs["moveId_val2"]->buffer);
	move->anim_len = (uint32_t)atoi(inputs["anim_len"]->buffer);
	move->airborne_start = (uint32_t)atoi(inputs["airborne_start"]->buffer);
	move->airborne_end = (uint32_t)atoi(inputs["airborne_end"]->buffer);
	move->ground_fall = (uint32_t)atoi(inputs["ground_fall"]->buffer);
	move->hitbox_location = (uint32_t)strtol(inputs["hitbox_location"]->buffer, nullptr, 16);
	move->last_active_frame = (uint32_t)atoi(inputs["last_active_frame"]->buffer);
	move->last_active_frame = (uint32_t)atoi(inputs["last_active_frame"]->buffer);
	move->distance = (uint16_t)atoi(inputs["distance"]->buffer);

	move->cancel_addr = atoll(inputs["cancel_id"]->buffer);
	move->hit_condition_addr = atoll(inputs["hit_condition_id"]->buffer);
	move->extra_move_property_addr = atoll(inputs["extra_properties_id"]->buffer);
	move->move_start_extraprop_addr = atoll(inputs["beginning_extra_properties_id"]->buffer);
	move->move_end_extraprop_addr = atoll(inputs["ending_extra_properties_id"]->buffer);
	move->voicelip_addr = atoll(inputs["voiceclip_id"]->buffer);

	move->_0x28_cancel_addr = atoll(inputs["cancel_id_2"]->buffer);
	move->_0x30_int__0x28_related = atoi(inputs["cancel_id_2_related"]->buffer);
	move->_0x38_cancel_addr = atoll(inputs["cancel_id_3"]->buffer);
	move->_0x40_int__0x38_related = atoi(inputs["cancel_id_3_related"]->buffer);
	move->_0x48_cancel_addr = atoll(inputs["cancel_id_4"]->buffer);
	move->_0x50_int__0x48_related = atoi(inputs["cancel_id_4_related"]->buffer);

	move->_0x34_int = atoi(inputs["_0x34_int"]->buffer);
	move->_0x44_int = atoi(inputs["_0x44_int"]->buffer);
	move->_0x56_short = atoi(inputs["_0x56_short"]->buffer);
	move->_0x5C_short = atoi(inputs["_0x5C_short"]->buffer);
	move->_0x5E_short = atoi(inputs["_0x5E_short"]->buffer);
	move->_0x98_int = atoi(inputs["_0x98_int"]->buffer);
	move->_0xA8_short = atoi(inputs["_0xA8_short"]->buffer);
	move->_0xAC_short = atoi(inputs["_0xAC_short"]->buffer);
}

bool EditorT7::ValidateMoveField(std::string name, EditorInput* field)
{
	if (name == "anim_name") {
		return m_animNameMap.find(field->buffer) != m_animNameMap.end();
	}

	else if (name == "cancel_id" || name == "cancel_id_2" ||
		name == "cancel_id_3" || name == "cancel_id_4") {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.cancelCount;
	}

	else if (name == "hit_condition_id") {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.hitConditionCount;
	}

	else if (name == "extra_properties_id") {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.extraMovePropertyCount;
	}

	else if (name == "beginning_extra_properties_id") {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.moveBeginningPropCount;
	}

	else if (name == "ending_extra_properties_id") {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.moveEndingPropCount;
	}

	else if (name == "voiceclip_id") {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.voiceclipCount;
	}

	else if (name == "transition") {
		int moveId = atoi(field->buffer);
		if (moveId >= m_infos->table.moveCount) {
			if (moveId < 0x8000) {
				return false;
			}
			if (moveId >= (0x8000 + m_aliases.size())) {
				return false;
			}
		}else if (moveId < 0) {
			return false;
		}
	}

	return true;
}

// ===== Generic =====

bool EditorT7::ValidateField(std::string fieldType, std::string fieldShortName, EditorInput* field)
{
	if (field->buffer[0] == '\0') {
		// There are no fields that should be allowed to be empty
		return false;
	}

	auto flags = field->flags;

	if (flags & EditorInput_H32) {
		if (strlen(field->buffer) > 8) {
			return false;
		}
	}
	else if (flags & EditorInput_H16) {
		if (strlen(field->buffer) > 4) {
			return false;
		}
	}
	else if (flags & (EditorInput_U64 | EditorInput_S64)) {
		if (strlen(field->buffer) > 19) {
			return false;
		}
		if ((flags & EditorInput_U64) && atoll(field->buffer) < 0) {
			return false;
		}
	}
	else if (flags & EditorInput_U32) {
		long long num = atoll(field->buffer);
		if (num > UINT_MAX || num < 0) {
			return false;
		}
	}
	else if (flags & EditorInput_S32) {
		long long num = atoll(field->buffer);
		if (num > INT_MAX || num < INT_MIN) {
			return false;
		}
	}
	else if (flags & EditorInput_U16) {
		int num = atoi(field->buffer);
		if (num > USHRT_MAX || num < 0) {
			return false;
		}
	}
	else if (flags & EditorInput_S16) {
		int num = atoi(field->buffer);
		if (num > SHRT_MAX || num < SHRT_MIN) {
			return false;
		}
	}

	if (fieldType == "move") {
		return ValidateMoveField(fieldShortName, field);
	}
	else if (fieldType == "voiceclip") {
		return ValidateVoiceclipField(field);
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
	for (uint16_t i = 0; i < 112 + 36; ++i) {
		m_aliases.push_back(aliasesPtr[i]);
	}

	// Build anim name : offset list
	uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
	gAddr::Move* movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);
	char const* namePtr = (char const*)(m_movesetData + m_header->offsets.nameBlock);
	
	for (size_t i = 0; i < m_infos->table.moveCount; ++i)
	{
		const char* animName = namePtr + movePtr[i].anim_name_addr;
		gameAddr animOffset = movePtr[i].anim_addr;

		if (m_animNameMap.find(animName) != m_animNameMap.end() && m_animNameMap[animName] != animOffset) {
			printf("move id %d\n", i);
			printf("Error: The same animation name refers to two different offsets. [%s] = [%llx] and [%llx]\n", animName, animOffset, m_animNameMap[animName]);
			throw std::exception();
		}

		m_animNameMap[animName] = animOffset;
		m_animNameMap_reverse[animOffset] = animName;
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