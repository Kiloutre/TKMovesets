# include "Editor_t7.hpp"
# include "Helpers.hpp"

#define gAddr StructsT7_gameAddr

void EditorT7::LoadMoveset(Byte* t_moveset, uint64_t t_movesetSize)
{
	m_moveset = t_moveset;
	m_movesetSize = t_movesetSize;

	// Start getting pointers toward useful data structures
	// Also get the actual game-moveset (past our header) pointer
	m_header = (TKMovesetHeader*)t_moveset;
	m_movesetData = t_moveset + m_header->offsets.movesetInfoBlock + m_header->infos.header_size;
	m_infos = (MovesetInfo*)m_movesetData;
}

EditorTable EditorT7::GetMovesetTable()
{
	std::vector<uint16_t> aliases;
	return EditorTable{
		.aliases = aliases,
	};
}

std::vector<DisplayableMove*> EditorT7::GetDisplayableMoveList()
{
	std::vector<DisplayableMove*> moves;
	
	uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
	gAddr::Move* movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);
	char const* namePtr = (char const*)(m_movesetData + m_header->offsets.nameBlock);

	uint16_t* aliases = m_infos->aliases1;
	uint8_t aliasesCount = 112 + 36;// sizeof(m_infos->aliases1);

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
		if (Helpers::startsWith(moveName, "c_")) {
			flags |= EditorMoveFlags_Custom;
		}

		moves.push_back(new DisplayableMove{
			.moveId = moveId,
			.name = moveName,
			.aliasId = aliasId,
			.flags = flags
		});
	}

	return moves;
}
