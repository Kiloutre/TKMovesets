#include <map>
#include <format>

# include "Editor_t7.hpp"
# include "Helpers.hpp"

#define gAddr StructsT7_gameAddr

void EditorT7::ModifyCancelListSize(uint32_t listId, uint16_t oldSize, uint16_t newSize)
{
	const int listSizeDiff = newSize - oldSize;
	const int structSize = sizeof(Cancel);
	const int structListSize = structSize * newSize;
	const int structListSizeDiff = structSize * listSizeDiff;

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t listOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock + (uint64_t)m_infos->table.cancel + listId * structSize;

	// todo: maybe align to 8 bytes in case the struct size is divisible by 4 and not 8. This is to keep following blocks 8 bytes aligned.
	//newMovesetSize = m_movesetSize + Helpers::align8Bytes(structListSizeDiff);
	newMovesetSize = m_movesetSize + structListSizeDiff;
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return ;
	}

	// Copy all the data up to the structure list 
	memcpy(newMoveset, m_moveset, listOffset);

	// Copy all the data after the structure list
	uint64_t postListOffset = listOffset + structSize * newSize;
	uint64_t orig_postListOffset = listOffset + structSize * oldSize;
	memcpy(newMoveset + postListOffset, m_moveset + orig_postListOffset, m_movesetSize - orig_postListOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	m_header->offsets.animationBlock += structListSizeDiff;
	m_header->offsets.motaBlock += structListSizeDiff;
	m_infos->table.cancelCount += listSizeDiff;

	uint64_t* countOffset = (uint64_t*)&m_infos->table;
	for (size_t i = 0; i < sizeof(MovesetTable) / 8 / 2; ++i)
	{
		if (*countOffset > (uint64_t)m_infos->table.cancel) {
			*countOffset += structListSizeDiff;
		}
		countOffset += 2;
	}

	// Correct moves that use cancel IDs after the one we modified
	uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
	gAddr::Move* movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);

	uint16_t moveId = 0;
	for (gAddr::Move* move = movePtr; moveId < m_infos->table.moveCount; ++moveId, ++move)
	{
		if (move->cancel_addr >= listId + oldSize || (listSizeDiff < 0 && move->cancel_addr > listId)) {
			move->cancel_addr += listSizeDiff;
		}
	}
}

void EditorT7::ModifyListSize(EditorWindowType_ type, uint32_t listId, uint16_t oldSize, uint16_t newSize)
{
	switch (type)
	{
	case EditorWindowType_Requirement:
		break;

	case EditorWindowType_Cancel:
		ModifyCancelListSize(listId, oldSize, newSize);
		break;
	case EditorWindowType_GroupedCancel:
		break;

	case EditorWindowType_Extraproperty:
		break;
	case EditorWindowType_MoveBeginProperty:
		break;
	case EditorWindowType_MoveEndProperty:
		break;

	case EditorWindowType_HitCondition:
		break;
	}
}