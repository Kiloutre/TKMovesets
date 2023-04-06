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

template<typename T> int EditorT7::ModifyGenericMovelistListSize2(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds, uint64_t listStart_offset)
{
	// Compute list old and new sizes
	int newSize = ids.size();
	int sizeDiff;
	int oldSize;
	{
		int newItemCount = 0;
		for (auto& id : ids)
		{
			if (id == -1) {
				newItemCount += 1;
			}
		}
		oldSize = (ids.size() - newItemCount) + deletedIds.size();
		sizeDiff = newSize - oldSize;
	}

	// Compute positions that will be used for copying
	int structSizeDiff = sizeof(T) * sizeDiff;
	uint64_t listPosition = listStart_offset + (sizeof(T) * listStart);
	uint64_t new_listEndPosition = listPosition + sizeof(T) * newSize;
	uint64_t old_listEndPosition = listPosition + sizeof(T) * oldSize;

	// Compute following m_header->offsets block position. We do this because we want to make sure it always stays 8 bytes aligned
	uint64_t new_followingBlockStart = m_movesetSize + structSizeDiff;
	uint64_t old_followingBlockStart = m_movesetSize;
	for (int i = 0; i < _countof(m_header->offsets.blocks); ++i)
	{
		if ((m_header->infos.header_size + m_header->offsets.blocks[i]) >= old_listEndPosition) {
			old_followingBlockStart = m_header->infos.header_size + m_header->offsets.blocks[i];
			new_followingBlockStart = Helpers::align8Bytes(old_followingBlockStart + structSizeDiff);
			break;
		}
	}

	// ------------------------------------------------
	// Compute new moveset size
	uint64_t newMovesetSize = new_followingBlockStart + (m_movesetSize - old_followingBlockStart);
	printf("Old size: %llx, new size: %llx\n", m_movesetSize, newMovesetSize);

	// Allocate new moveset
	Byte* newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return 0;
	}

	// ------------------------------------------------
	// Before we begin copying, there are a few values we want updated in the new moveset before it gets copied loaded up

	// Update count & table offsets right now so that iterators built from LoadMovesetPtr() are up to date
	/*
	{
		uint64_t movesetBlockStart = (m_header->infos.header_size + m_header->offsets.movesetBlock);
		uint64_t movesetBlockEnd = movesetBlockStart + (m_header->offsets.animationBlock - m_header->offsets.movesetBlock);

		if (movesetBlockStart < listPosition && listPosition < movesetBlockEnd)
		{
			unsigned int tableEntryCount = _countof(m_infos->table.entries);
			for (size_t i = 0; i < tableEntryCount; ++i)
			{
				auto& currentEntry = m_infos->table.entries[i];
				gameAddr absolute_entryStartAddr = movesetBlockStart + (uint64_t)currentEntry.offset;
			
				// listPosition be equal to a list's start both with and and without belonging to the list
				// Rule out the case where an empty structure list (count = 0) is resized into more
				// This is overkill and not likely to happen since ModifyListSize() is never called for list creations but modification of existing ones
				if (listPosition < absolute_entryStartAddr || \
					(listPosition == absolute_entryStartAddr && currentEntry.count != 0)) {
					*(uint64_t*)&currentEntry.offset += structSizeDiff;
					DEBUG_LOG("Shifting movelist entry %d's offset\n", i);
				}
				else {
					bool isPartOfCurrentList = false;

					if (i + 1 == tableEntryCount) {
						isPartOfCurrentList = true;
					}
					else {
						auto& nextEntry = m_infos->table.entries[i + 1];
						gameAddr absolute_nextEntryStartAddr = movesetBlockStart + (uint64_t)nextEntry.offset;

						if (listPosition < absolute_nextEntryStartAddr || \
							(listPosition == absolute_nextEntryStartAddr && nextEntry.count != 0)) {
							isPartOfCurrentList = true;
						}
					}

					if (isPartOfCurrentList) {
						currentEntry.count += sizeDiff;
						DEBUG_LOG("Adding movelist entry %llu count += %d\n", i, sizeDiff);
					}
				}
			}
		}
	}
	*/

	// There are blocks to shift in the displayble movelist block
	{
		uint64_t movelistBlockStart = (m_header->infos.header_size + m_header->offsets.movelistBlock);
		uint64_t movelistBlockEnd = m_movesetSize;

		if (movelistBlockStart < listPosition && listPosition < movelistBlockEnd && strncmp(m_mvlHead->mvlString, "MVLT", 4) == 0)
		{
			uint64_t relativeOffset = listPosition - movelistBlockStart;

			if (m_mvlHead->inputs_offset > relativeOffset) {
				m_mvlHead->inputs_offset += structSizeDiff;
				DEBUG_LOG("MVLT: Shifted mvl_inputs block by %d (0x%x) bytes\n", structSizeDiff, structSizeDiff);

				if (m_mvlHead->playables_offset > relativeOffset) {
					m_mvlHead->playables_offset += structSizeDiff;
					DEBUG_LOG("MVTL: Shifted mvl_playable block by %d (0x%x) bytes\n", structSizeDiff, structSizeDiff);

					if (m_mvlHead->displayables_offset > relativeOffset) {
						m_mvlHead->displayables_offset += structSizeDiff;
						DEBUG_LOG("MVLT: Shifted mvl_displayable block %d (0x%x) bytes\n", structSizeDiff, structSizeDiff);
					}
				}
			}
		}
	}

	// Global moveset blocks must be shifted, better here than later
	for (int i = 0; i < _countof(m_header->offsets.blocks); ++i)
	{
		if ((m_header->infos.header_size + m_header->offsets.blocks[i]) >= listPosition) {
			m_header->offsets.blocks[i] += structSizeDiff;
			DEBUG_LOG("Shifted moveset block %d by 0x%x\n", i, structSizeDiff);
		}
	}

	// ------------------------------------------------
	// Copy up to list start
	memcpy(newMoveset, m_moveset, listPosition);

	// Copy list items, one by one
	// That way mere reordering + resize technically don't need SaveItem() to be re-applied
	{
		uint64_t writeOffset = listPosition - sizeof(T);
		uint64_t readOffset;
		auto it = deletedIds.begin();
		for (auto id : ids)
		{
			writeOffset += sizeof(T);

			if (id != -1) {
				// Copy old structure data from its previous position (likely to not have changed but this handles reodering just fine)
				readOffset = listPosition + (sizeof(T) * id);
			}
			else {
				// Try to use existing data from deleted lists. This can be useful in rare cases like MvlDisplayables.
				if (it != deletedIds.end()) {
					readOffset = listPosition + (sizeof(T) * id);
					std::advance(it, 1);
				}
				else {
					printf("--continue\n");
					// Don't bother copying anything
					continue;
				}
			}

			memcpy(newMoveset + writeOffset, m_moveset + readOffset, sizeof(T));
		}
	}

	// Copy everything after the list up to the next block
	memcpy(newMoveset + new_listEndPosition, m_moveset + old_listEndPosition, old_followingBlockStart - old_listEndPosition);
	printf("%llx - %llx = %llx\n", old_followingBlockStart, old_listEndPosition, old_followingBlockStart - old_listEndPosition);
	printf("memcpy from %llx to %llx\n", new_listEndPosition, new_listEndPosition + (old_followingBlockStart - old_listEndPosition));

	// If there is a block afterward and not just the moveset end, copy everything from that block to the moveset end
	if (old_followingBlockStart != m_movesetSize) {
		printf("memcpy %llx + %llx, %lld (%llx)\n", newMoveset, new_followingBlockStart, m_movesetSize - old_followingBlockStart);
		memcpy(newMoveset + new_followingBlockStart, m_moveset + old_followingBlockStart, m_movesetSize - old_followingBlockStart);
	}

	// Free up old moveset from memory and assign the new one
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	return sizeDiff;
}

void EditorT7::ModifyMovelistDisplayableSize2(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{

	uint64_t listHead = m_header->infos.header_size + m_header->offsets.movelistBlock + m_mvlHead->displayables_offset;
	int listSizeDiff = ModifyGenericMovelistListSize2<MvlDisplayable>(listStart, ids, deletedIds, listHead);

	// Update count and iterator's count
	m_mvlHead->displayables_count += listSizeDiff;
	SetupIterators_DisplayableMovelist();
}

void EditorT7::ModifyListSize2(EditorWindowType_ type, unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	switch (type)
	{
	case EditorWindowType_MovelistDisplayable:
		ModifyMovelistDisplayableSize2(listStart, ids, deletedIds);
		break;
	}
}