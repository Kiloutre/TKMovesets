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
	// todo
	// this is a lot more complex than it seems because we don't know which item in the list moved
	/*
	ModifyGenericMovelistListSize<MvlDisplayable>(listId, oldSize, newSize, m_mvlHead->displayables_offset);

	// Shift translation string offsets that are relative to displayable struct addresses
	const int listSizeDiff = (newSize - oldSize) * sizeof(MvlDisplayable);
	for (int i = 0; i < m_iterators.mvl_displayables.size(); ++i)
	{
		auto displayable = m_iterators.mvl_displayables[i];

		for (int i = 0; i < _countof(displayable.translationOffsets); ++i) {
			// todo: check if there is a need to shift
			displayable.translationOffsets[i] -= listSizeDiff;
		}
	}
	*/
}

void EditorT7::ModifyMovelistInputSize(int listId, int oldSize, int newSize)
{
	ModifyGenericMovelistListSize<MvlInput>(listId, oldSize, newSize, m_mvlHead->inputs_offset);

	// Shift input offsets that are relative to playable struct addresses
	const int32_t listSizeDiff = (newSize - oldSize) * sizeof(MvlInput);
	const uint32_t absoluteListAddr = m_mvlHead->inputs_offset + (listId * sizeof(MvlInput));
	int playableIdx = 0;
	for (auto& playable : m_iterators.mvl_playables)
	{
		uint32_t absolutePlayableAddr = m_mvlHead->playables_offset + (playableIdx++ * sizeof(MvlPlayable));
		uint32_t playable_inputAbsoluteOffset = (absolutePlayableAddr + playable.input_sequence_offset);
		if (playable_inputAbsoluteOffset > absoluteListAddr) {
			playable.input_sequence_offset += listSizeDiff;
		}
		else if (playable_inputAbsoluteOffset == absoluteListAddr) {
			playable.input_count += (newSize - oldSize);
		}
	}
}