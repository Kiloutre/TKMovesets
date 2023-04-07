# include "Editor_t7.hpp"
# include "Helpers.hpp"

#define gAddr StructsT7_gameAddr

// I end up using this only or strings, but it works pretty well
void EditorT7::DisplayableMVLTranslationReallocate(int listId, int oldSize, int newSize, uint32_t listStart_offset)
{
	const int listSizeDiff = newSize - oldSize;
	const uint64_t structSize = sizeof(char);
	const int structListSize = structSize * newSize;
	const int structListSizeDiff = structSize * listSizeDiff;

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t listOffset = m_header->infos.header_size + m_header->offsets.movelistBlock + listStart_offset + ((uint64_t)listId * structSize);

	uint64_t postListOffset = listOffset + structSize * newSize;
	uint64_t orig_postListOffset = listOffset + structSize * oldSize;

	// Compute following m_header->offsets block position. We do this because we want to make sure it always stays 8 bytes aligned
	uint64_t new_followingBlockStart = m_movesetSize + structListSizeDiff;
	uint64_t old_followingBlockStart = m_movesetSize;
	for (int i = 0; i < _countof(m_header->offsets.blocks); ++i)
	{
		uint64_t absoluteBlockAddr = (m_header->infos.header_size + m_header->offsets.blocks[i]);
		if (absoluteBlockAddr >= orig_postListOffset) {
			old_followingBlockStart = absoluteBlockAddr;
			new_followingBlockStart = Helpers::align8Bytes(old_followingBlockStart + structListSizeDiff);
			break;
		}
	}

	newMovesetSize = m_movesetSize + (new_followingBlockStart - old_followingBlockStart);
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		throw;
	}

	// Shift offsets in the mvl head right away so that iterators can work later on
	uint64_t orig_relative_postLisOffset = orig_postListOffset - (m_header->infos.header_size + m_header->offsets.movelistBlock);

	if (m_mvlHead->inputs_offset >= orig_relative_postLisOffset) {
		m_mvlHead->inputs_offset += structListSizeDiff;

		if (m_mvlHead->playables_offset >= orig_relative_postLisOffset) {
			m_mvlHead->playables_offset += structListSizeDiff;
			DEBUG_LOG("MVLT: Shifted mvl_inputs block by %d (0x%x) bytes\n", structListSizeDiff, structListSizeDiff);

			if (m_mvlHead->displayables_offset >= orig_relative_postLisOffset) {
				m_mvlHead->displayables_offset += structListSizeDiff;
				DEBUG_LOG("MVLT: Shifted mvl_playables block by %d (0x%x) bytes\n", structListSizeDiff, structListSizeDiff);
			}
		}
	}

	// Shift moveset blocks offsets if necessary (at the time of this commit, it isn't)
	for (int i = 0; i < _countof(m_header->offsets.blocks); ++i)
	{
		if ((m_header->infos.header_size + m_header->offsets.blocks[i]) >= orig_postListOffset) {
			m_header->offsets.blocks[i] = Helpers::align8Bytes(m_header->offsets.blocks[i] + structListSizeDiff);
			DEBUG_LOG("Shifted moveset block %d by 0x%x\n", i, structListSizeDiff);
		}
	}

	// Copy all the data up to the structure list 
	memcpy(newMoveset, m_moveset, listOffset);

	// Copy all the data after the structure list
	memcpy(newMoveset + postListOffset, m_moveset + orig_postListOffset, old_followingBlockStart - orig_postListOffset);

	// If there is a block afterward and not just the moveset end, copy everything from that block to the moveset end
	if (old_followingBlockStart != m_movesetSize) {
		memcpy(newMoveset + new_followingBlockStart, m_moveset + old_followingBlockStart, m_movesetSize - old_followingBlockStart);
	}

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);
}

template<typename T> int EditorT7::ModifyGenericMovelistListSize2(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds, uint64_t listStart_offset)
{
	// Compute list old and new sizes
	int newSize = (int)ids.size();
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
		oldSize = ((int)ids.size() - newItemCount) + (int)deletedIds.size();
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
		uint64_t absoluteBlockAddr = (m_header->infos.header_size + m_header->offsets.blocks[i]);
		if (absoluteBlockAddr >= old_listEndPosition) {
			old_followingBlockStart = absoluteBlockAddr;
			new_followingBlockStart = Helpers::align8Bytes(old_followingBlockStart + structSizeDiff);
			break;
		}
	}

	// ------------------------------------------------
	// Compute new moveset size
	uint64_t newMovesetSize = new_followingBlockStart + (m_movesetSize - old_followingBlockStart);

	// Allocate new moveset
	Byte* newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return 0;
	}

	// ------------------------------------------------
	// Before we begin copying, there are a few values we want updated in the new moveset before it gets copied loaded up

	// Update count & table offsets right now so that iterators built from LoadMovesetPtr() are up to date
	{
		uint64_t movesetBlockStart = (m_header->infos.header_size + m_header->offsets.movesetBlock);
		uint64_t movesetBlockEnd = movesetBlockStart + (m_header->offsets.animationBlock - m_header->offsets.movesetBlock);

		if (movesetBlockStart < listPosition && listPosition < movesetBlockEnd)
		{
			unsigned int tableEntryCount = _countof(m_infos->table.entries);
			for (size_t i = 0; i < tableEntryCount; ++i)
			{
				auto& currentEntry = m_infos->table.entries[i];
				gameAddr absolute_entryStartAddr = movesetBlockStart + (uint64_t)currentEntry.listAddr;
			
				// listPosition be equal to a list's start both with and and without belonging to the list
				// Rule out the case where an empty structure list (count = 0) is resized into more
				// This is overkill and not likely to happen since ModifyListSize() is never called for list creations but modification of existing ones
				if (listPosition < absolute_entryStartAddr || \
					(listPosition == absolute_entryStartAddr && currentEntry.listCount != 0)) {
					*(uint64_t*)&currentEntry.listAddr += structSizeDiff;
					DEBUG_LOG("Shifting movesetblock entry %lld's offset\n", i);
				}
				else {
					bool isPartOfCurrentList = false;

					if (i + 1 == tableEntryCount) {
						isPartOfCurrentList = true;
					}
					else {
						auto& nextEntry = m_infos->table.entries[i + 1];
						gameAddr absolute_nextEntryStartAddr = movesetBlockStart + (uint64_t)nextEntry.listAddr;

						if (listPosition < absolute_nextEntryStartAddr || \
							(listPosition == absolute_nextEntryStartAddr && nextEntry.listCount != 0)) {
							isPartOfCurrentList = true;
						}
					}

					if (isPartOfCurrentList) {
						currentEntry.listCount += sizeDiff;
						DEBUG_LOG("Adding movelist entry %llu count += %d\n", i, sizeDiff);
					}
				}
			}
		}
	}

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
			m_header->offsets.blocks[i] = Helpers::align8Bytes(m_header->offsets.blocks[i] + structSizeDiff);
			DEBUG_LOG("Shifted moveset block %d by 0x%x (before alignment)\n", i, structSizeDiff);
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
					// Don't bother copying anything. Calloc() implies that every byte of this struct will be at 0 anyway, so rely on that and do not change it.
					continue;
				}
			}

			memcpy(newMoveset + writeOffset, m_moveset + readOffset, sizeof(T));
		}
	}

	// Copy everything after the list up to the next block
	memcpy(newMoveset + new_listEndPosition, m_moveset + old_listEndPosition, old_followingBlockStart - old_listEndPosition);

	// If there is a block afterward and not just the moveset end, copy everything from that block to the moveset end
	if (old_followingBlockStart != m_movesetSize) {
		memcpy(newMoveset + new_followingBlockStart, m_moveset + old_followingBlockStart, m_movesetSize - old_followingBlockStart);
	}

	// Free up old moveset from memory and assign the new one
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	return sizeDiff;
}

void EditorT7::ModifyMovelistDisplayableTextSize(int listId, int oldSize, int newSize)
{
	// Might as well use this generic method for strings since we don't care about alignment here
	const int listSizeDiff = newSize - oldSize;
	DisplayableMVLTranslationReallocate(listId, oldSize, newSize, 0);

	int32_t offset = listId;
	for (auto& displayable : m_iterators.mvl_displayables)
	{
		for (int i = 0; i < _countof(displayable.all_translation_offsets); ++i) {
			if (displayable.all_translation_offsets[i] > listId) {
				displayable.all_translation_offsets[i] += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyMovelistDisplayableSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	std::map<uint32_t, uint32_t> deletedTranslatons;

	// Build the list of translation strings we will deallocate
	for (auto id : deletedIds)
	{
		auto displayable = m_iterators.mvl_displayables[id];
		for (size_t i = 0; i < _countof(displayable->all_translation_offsets); ++i) {
			uint32_t addr = displayable->all_translation_offsets[i];
			uint32_t addrEnd = 0;

			if (deletedTranslatons.find(addr) != deletedTranslatons.end()) {
				// Already in it somehow
				// Ensure that the biggest size is saved
				if (addrEnd < deletedTranslatons[addr]) {
					continue;
				}
			}
			deletedTranslatons[addr] = (uint32_t)strlen((char*)m_movesetData + m_header->offsets.movelistBlock + addr);
		}
	}

	// Deallocate the later addresses first to avoid having to take into account shifting
	for (auto iter = deletedTranslatons.rbegin(); iter != deletedTranslatons.rend(); ++iter)
	{
		uint32_t stringAddr_start = iter->first;
		uint32_t stringAddr_size = iter->second;
		uint32_t stringAddr_end = stringAddr_start + stringAddr_size;

		// Have to painstainingly ensure the string isn't used anywhere else
		bool stillUsed = false;
		int idx = 0;
		for (auto& displayable : m_iterators.mvl_displayables)
		{
			if (deletedIds.find(idx++) != deletedIds.end()) {
				continue;
			}

			for (size_t i = 0; i < _countof(displayable.all_translation_offsets); ++i)
			{
				uint64_t currentStringAddr = displayable.all_translation_offsets[i];
				if (stringAddr_start <= currentStringAddr && currentStringAddr <= stringAddr_end) {
					stillUsed = true;
					break;
				}
			}
		}

		if (!stillUsed) {
			DisplayableMVLTranslationReallocate(stringAddr_start, stringAddr_size, 0, 0);

			// Have to shift every reference now
			int idx = 0;
			for (auto& displayable : m_iterators.mvl_displayables)
			{
				if (deletedIds.find(idx++) != deletedIds.end()) {
					continue;
				}
				for (int i = 0; i < _countof(displayable.all_translation_offsets); ++i) {
					if (displayable.all_translation_offsets[i] >= (int32_t)stringAddr_start) {
						displayable.all_translation_offsets[i] -= (int32_t)stringAddr_size;
					}
				}
			}
		}
	}

	uint64_t listHead = m_header->infos.header_size + m_header->offsets.movelistBlock + m_mvlHead->displayables_offset;
	int listSizeDiff = ModifyGenericMovelistListSize2<MvlDisplayable>(listStart, ids, deletedIds, listHead);

	// Update count and iterator's count
	m_mvlHead->displayables_count += listSizeDiff;
	SetupIterators_DisplayableMovelist();
}

void EditorT7::ModifyMovelistInputSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{

	uint64_t listHead = m_header->infos.header_size + m_header->offsets.movelistBlock + m_mvlHead->inputs_offset;
	int listSizeDiff = ModifyGenericMovelistListSize2<MvlInput>(listStart, ids, deletedIds, listHead);

	// No need to update count for input list cause we never store the count of the full thing, just like the game

	int oldSize = (int)ids.size() + listSizeDiff;
	for (auto& playable : m_iterators.mvl_playables)
	{
		// Todo: now that we know exactly whidh IDs is getting deleted, we can set the values properly
		if (MUST_SHIFT_ID(playable.input_sequence_offset, listSizeDiff, (int)listStart, (int)listStart + oldSize)) {
			playable.input_sequence_offset += listSizeDiff;
		}
		else if ((int)playable.input_sequence_offset >= listStart && playable.input_sequence_offset <= ((uint64_t)listStart + oldSize)) {
			playable.input_count += listSizeDiff;
		}
	}
}

void EditorT7::ModifyListSize2(EditorWindowType_ type, unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	switch (type)
	{
	case EditorWindowType_MovelistDisplayable:
		ModifyMovelistDisplayableSize(listStart, ids, deletedIds);
		break;

	case EditorWindowType_MovelistInput:
		ModifyMovelistInputSize(listStart, ids, deletedIds);
		break;
	}
}