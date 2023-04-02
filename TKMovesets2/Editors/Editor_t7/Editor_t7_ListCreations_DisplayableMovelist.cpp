# include "Editor_t7.hpp"
# include "Helpers.hpp"

#define gAddr StructsT7_gameAddr

template<typename T> void EditorT7::ModifyGenericMovelistListSize(int listId, int oldSize, int newSize, uint32_t listStart_offset)
{
	const int listSizeDiff = newSize - oldSize;
	const uint64_t structSize = sizeof(T);
	const int structListSize = structSize * newSize;
	const int structListSizeDiff = structSize * listSizeDiff;

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t listOffset = m_header->infos.header_size + m_header->offsets.movelistBlock + listStart_offset + (uint64_t)listId * structSize;

	uint64_t postListOffset = listOffset + structSize * newSize;
	uint64_t orig_postListOffset = listOffset + structSize * oldSize;

	// todo: maybe align to 8 bytes in case the struct size is divisible by 4 and not 8. This is to keep following blocks 8 bytes aligned.
	//newMovesetSize = m_movesetSize + Helpers::align8Bytes(structListSizeDiff);
	newMovesetSize = m_movesetSize + structListSizeDiff;
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return;
	}

	// Shift offsets in the mvl head right away so that iterators can work later on
	if (m_mvlHead->displayables_offset >= orig_postListOffset) {
		m_mvlHead->displayables_offset += structListSizeDiff;

		if (m_mvlHead->playables_offset >= orig_postListOffset) {
			m_mvlHead->playables_offset += structListSizeDiff;

			if (m_mvlHead->inputs_offset >= orig_postListOffset) {
				m_mvlHead->inputs_offset += structListSizeDiff;
			}
		}
	}

	// Copy all the data up to the structure list 
	memcpy(newMoveset, m_moveset, listOffset);

	// Copy all the data after the structure list
	memcpy(newMoveset + postListOffset, m_moveset + orig_postListOffset, m_movesetSize - orig_postListOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);
}

void EditorT7::ModifyMovelistDisplayableSize(int listId, int oldSize, int newSize)
{
	ModifyGenericMovelistListSize<MvlDisplayable>(listId, oldSize, newSize, m_mvlHead->displayables_offset);

	// No correction to do for once since no one refers to the displayable lists by anythign else than its head
}

void EditorT7::ModifyMovelistInputSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericMovelistListSize<MvlInput>(listId, oldSize, newSize, m_mvlHead->inputs_offset);
	//;
	for (auto& playable : m_iterators.mvl_playables)
	{
		if (MUST_SHIFT_ID(playable.input_sequence_offset, listSizeDiff, listId, listId + oldSize)) {
			playable.input_sequence_offset += listSizeDiff;
		}
		else if ((int)playable.input_sequence_offset >= listId && playable.input_sequence_offset <= ((uint64_t)listId + oldSize)) {
			playable.input_sequence_offset += listSizeDiff;
		}
	}
}