# include "Editor_t7.hpp"
# include "Helpers.hpp"

#define gAddr StructsT7_gameAddr

template<typename T> void EditorT7::ModifyGenericMovelistListSize(int listId, int oldSize, int newSize, uint32_t listStart_offset)
{
	//printf("ModifyGenericMovelistListSize %d %d %d\n", listId, oldSize, newSize);
	const int listSizeDiff = newSize - oldSize;
	const uint64_t structSize = sizeof(T);
	const int structListSize = structSize * newSize;
	const int structListSizeDiff = structSize * listSizeDiff;

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t listOffset = m_header->infos.header_size + m_header->offsets.movelistBlock + listStart_offset + ((uint64_t)listId * structSize);

	uint64_t postListOffset = listOffset + structSize * newSize;
	uint64_t orig_postListOffset = listOffset + structSize * oldSize;

	newMovesetSize = m_movesetSize + structListSizeDiff;
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		throw;
	}

	// Shift offsets in the mvl head right away so that iterators can work later on
	uint64_t orig_relative_postLisOffset = orig_postListOffset - (m_header->infos.header_size + m_header->offsets.movelistBlock);

	if (m_mvlHead->inputs_offset >= orig_relative_postLisOffset) {
		m_mvlHead->inputs_offset += structListSizeDiff;
		//DEBUG_LOG("Shifted mvl_inputs block  0x%x\n", structListSizeDiff);

		if (m_mvlHead->playables_offset >= orig_relative_postLisOffset) {
			m_mvlHead->playables_offset += structListSizeDiff;
			//DEBUG_LOG("Shifted mvl_playable block  0x%x\n", structListSizeDiff);

			if (m_mvlHead->displayables_offset >= orig_relative_postLisOffset) {
				m_mvlHead->displayables_offset += structListSizeDiff;
				//DEBUG_LOG("Shifted mvl_displayable block  0x%x\n", structListSizeDiff);
			}
		}
	}

	// Shift moveset blocks offsets if necessary (at the time of this commit, it isn't)
	for (int i = 0; i < _countof(m_header->offsets.blocks); ++i)
	{
		if ((m_header->infos.header_size + m_header->offsets.blocks[i]) >= orig_postListOffset) {
			m_header->offsets.blocks[i] += structListSizeDiff;
			//DEBUG_LOG("Shifted moveset block %d by 0x%x\n", i, structListSizeDiff);
		}
	}

	// Copy all the data up to the structure list 
	memcpy(newMoveset, m_moveset, listOffset);
	//printf("Copy up to orig %llx\n", listOffset);

	// Copy all the data after the structure list
	memcpy(newMoveset + postListOffset, m_moveset + orig_postListOffset, m_movesetSize - orig_postListOffset);
	//printf("Copy from orig %llx at %llx, size %llx\n", orig_postListOffset, postListOffset, m_movesetSize - orig_postListOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);
}

void EditorT7::ModifyMovelistDisplayableTextSize(int listId, int oldSize, int newSize)
{
	// Might as well use this generic method for strings since we don't care about alignment here
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericMovelistListSize<char>(listId, oldSize, newSize, 0);

	int32_t offset = listId;
	for (auto& displayable : m_iterators.mvl_displayables)
	{
		/*
		for (int i = 0; i < _countof(displayable.title_translation_offsets); ++i) {
			if (displayable.title_translation_offsets[i] > listId) {
				displayable.title_translation_offsets[i] += listSizeDiff;
			}
		}
		*/

		for (int i = 0; i < _countof(displayable.translation_offsets); ++i) {
			if (displayable.translation_offsets[i] > listId) {
				printf("%d. increase of %d (curr is %d)\n", i, listSizeDiff, displayable.translation_offsets[i]);
				displayable.translation_offsets[i] += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyMovelistDisplayableSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericMovelistListSize<MvlDisplayable>(listId, oldSize, newSize, m_mvlHead->displayables_offset);

	m_mvlHead->displayables_count += listSizeDiff;
	// No correction to do for once since no one refers to the displayable lists by anythign else than its head
}

void EditorT7::ModifyMovelistInputSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericMovelistListSize<MvlInput>(listId, oldSize, newSize, m_mvlHead->inputs_offset);

	for (auto& playable : m_iterators.mvl_playables)
	{
		if (MUST_SHIFT_ID(playable.input_sequence_offset, listSizeDiff, listId, listId + oldSize)) {
			playable.input_sequence_offset += listSizeDiff;
		}
		else if ((int)playable.input_sequence_offset >= listId && playable.input_sequence_offset <= ((uint64_t)listId + oldSize)) {
			playable.input_count += listSizeDiff;
		}
	}
}