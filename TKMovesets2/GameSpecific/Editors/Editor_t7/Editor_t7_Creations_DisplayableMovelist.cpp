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

	const uint64_t listOffset = m_header->infos.moveset_data_start + m_header->offsets.movelistBlock + listStart_offset + ((uint64_t)listId * structSize);

	uint64_t postListOffset = listOffset + structSize * newSize;
	uint64_t orig_postListOffset = listOffset + structSize * oldSize;

	// Compute following m_header->offsets block position. We do this because we want to make sure it always stays 8 bytes aligned
	uint64_t new_followingBlockStart = m_movesetSize + structListSizeDiff;
	uint64_t old_followingBlockStart = m_movesetSize;
	for (int i = 0; i < _countof(m_header->offsets.blocks); ++i)
	{
		uint64_t absoluteBlockAddr = (m_header->infos.moveset_data_start + m_header->offsets.blocks[i]);
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
	uint64_t orig_relative_postLisOffset = orig_postListOffset - (m_header->infos.moveset_data_start + m_header->offsets.movelistBlock);

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
		if ((m_header->infos.moveset_data_start + m_header->offsets.blocks[i]) >= orig_postListOffset) {
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

	uint64_t listHead = m_header->infos.moveset_data_start + m_header->offsets.movelistBlock + m_mvlHead->displayables_offset;
	int listSizeDiff = ModifyGenericMovelistListSize<MvlDisplayable>(listStart, ids, deletedIds, listHead);

	// Update count and iterator's count
	m_mvlHead->displayables_count += listSizeDiff;
	SetupIterators_DisplayableMovelist();
}

void EditorT7::ModifyMovelistInputSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->infos.moveset_data_start + m_header->offsets.movelistBlock + m_mvlHead->inputs_offset;
	int listSizeDiff = ModifyGenericMovelistListSize<MvlInput>(listStart, ids, deletedIds, listHead);

	// No real need to update count for input list in term of structure data
	// I do it anyway because this avoids me having
	m_iterators.mvl_inputs.set_size(m_iterators.mvl_inputs.size() + listSizeDiff);

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

//

uint32_t EditorT7::CreateNewMvlPlayable()
{
	uint32_t newStructId = m_mvlHead->playables_count;
	uint64_t listHead = m_header->infos.moveset_data_start + m_header->offsets.movelistBlock + m_mvlHead->playables_offset;

	ModifyGenericMovelistListSize<MvlPlayable>(newStructId, { -1 }, {}, listHead);
	MvlPlayable* playable = m_iterators.mvl_playables[newStructId];
	playable->distance = 1000;

	m_mvlHead->playables_count++;
	SetupIterators_DisplayableMovelist();
	return newStructId;
}

uint32_t EditorT7::CreateNewMvlInputs()
{
	uint32_t newStructId = (uint32_t)m_iterators.mvl_inputs.size();
	uint64_t listHead = m_header->infos.moveset_data_start + m_header->offsets.movelistBlock + m_mvlHead->inputs_offset;

	ModifyGenericMovelistListSize<MvlInput>(newStructId, { -1, -1 }, {}, listHead);
	MvlInput* inputs = m_iterators.mvl_inputs[newStructId];
	inputs[0].frame_duration = 5;
	inputs[1].frame_duration = 5;

	m_iterators.mvl_inputs.set_size(newStructId + 2);
	return newStructId;
}