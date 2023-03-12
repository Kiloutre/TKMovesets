# include "Editor_t7.hpp"
# include "Helpers.hpp"

#define gAddr StructsT7_gameAddr

// -- Static helpers -- //

static std::vector<EditorInput*> GetMoveInputs()
{
	return {
		new EditorInput{
			.name = "edition.move_field.vulnerability",
			.offset = offsetof(Move, vuln),
			.memberSize = 4,
			.category = 0,
			.imguiInputFlags = ImGuiInputTextFlags_CharsDecimal,
			.flags = EditorInputType_unsigned
		},
		new EditorInput{
			.name = "edition.move_field.hitlevel",
			.offset = offsetof(Move, hitlevel),
			.memberSize = 4,
			.category = 0,
			.imguiInputFlags = ImGuiInputTextFlags_CharsDecimal,
			.flags = EditorInputType_unsigned
		},
		new EditorInput{
			.name = "edition.move_field.cancel_id",
			.offset = offsetof(Move, cancel_addr),
			.memberSize = 8,
			.category = 0,
			.imguiInputFlags = ImGuiInputTextFlags_CharsDecimal,
		},

		new EditorInput{
			.name = "edition.move_field.cancel_id_2",
			.offset = offsetof(Move, _0x28_cancel_addr),
			.memberSize = 8,
			.category = 1,
			.imguiInputFlags = ImGuiInputTextFlags_CharsDecimal,
		},
		new EditorInput{
			.name = "edition.move_field.cancel_id_2_related",
			.offset = offsetof(Move, _0x30_int__0x28_related),
			.memberSize = 4,
			.category = 1,
			.imguiInputFlags = ImGuiInputTextFlags_CharsDecimal,
			.flags = EditorInputType_unsigned
		},
		new EditorInput{
			.name = "edition.move_field.cancel_id_3",
			.offset = offsetof(Move, _0x38_cancel_addr),
			.memberSize = 8,
			.category = 1,
			.imguiInputFlags = ImGuiInputTextFlags_CharsDecimal,
		},
		new EditorInput{
			.name = "edition.move_field.cancel_id_3_related",
			.offset = offsetof(Move, _0x40_int__0x38_related),
			.memberSize = 4,
			.category = 1,
			.imguiInputFlags = ImGuiInputTextFlags_CharsDecimal,
			.flags = EditorInputType_unsigned
		},
		new EditorInput{
			.name = "edition.move_field.cancel_id_4",
			.offset = offsetof(Move, _0x48_cancel_addr),
			.memberSize = 8,
			.category = 1,
			.imguiInputFlags = ImGuiInputTextFlags_CharsDecimal,
		},
		new EditorInput{
			.name = "edition.move_field.cancel_id_4_related",
			.offset = offsetof(Move, _0x50_int__0x48_related),
			.memberSize = 4,
			.category = 1,
			.imguiInputFlags = ImGuiInputTextFlags_CharsDecimal,
			.flags = EditorInputType_unsigned
		},
	};
}

// -- Private methods -- //

// -- Public methods -- //

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

bool EditorT7::ValidateField(EditorInput* field)
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

	std::string fieldIdentifier = field->name;
	if (fieldIdentifier == "edition.move_field.cancel_id" ||
		fieldIdentifier == "edition.move_field.cancel_id_2" ||
		fieldIdentifier == "edition.move_field.cancel_id_3" ||
		fieldIdentifier == "edition.move_field.cancel_id_4") {
		int cancelIdx = atoi(field->buffer);
		if (cancelIdx < 0 || cancelIdx >= m_infos->table.cancelCount) {
			return false;
		}
	}
	return true;
}

std::vector<EditorInput*> EditorT7::GetFormInputs(std::string formIdentifier)
{
	if (formIdentifier == "move") {
		//uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
		//gAddr::Move* movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);
		return GetMoveInputs();
	}

	return std::vector<EditorInput*>();
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
	uint8_t aliasesCount = m_aliases.size();

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