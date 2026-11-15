# include "Editor_t7.hpp"
# include "Helpers.hpp"

using namespace StructsT7;
#define gAddr StructsT7_gameAddr



void EditorT7::ModifyMovelistDisplayableSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movelistBlock + m_mvlHead->displayables_offset;
	int listSizeDiff = ModifyGenericMovelistListSize<MvlDisplayable>(listStart, ids, deletedIds, listHead, DescribeRecordCount(m_mvlHead->displayables_count));

	// Update count and iterator's count
	SetupIterators_DisplayableMovelist();
}

void EditorT7::ModifyMovelistInputSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movelistBlock + m_mvlHead->inputs_offset;
	int listSizeDiff = ModifyGenericMovelistListSize<MvlInput>(listStart, ids, deletedIds, listHead, RecordListCount{m_iterators.mvl_inputs.size(), 0, 0});

	// No real need to update count for input list in term of structure data
	// I do it anyway because this avoids me having
	m_iterators.mvl_inputs.set_size(m_iterators.mvl_inputs.size() + listSizeDiff);


    RecordEdit edit(listStart, ids, deletedIds, m_iterators.mvl_inputs.size() - listSizeDiff);
    for (auto& playable : m_iterators.mvl_playables) {
        auto [first, amount] = edit.RemapRange(playable.input_sequence_offset, playable.input_count);
        if (first > UINT32_MAX || amount > UINT16_MAX) throw std::out_of_range("Movelist input sequence overflow");
        playable.input_sequence_offset = static_cast<uint32_t>(first);
        playable.input_count = static_cast<uint16_t>(amount);
    }
}

//

uint32_t EditorT7::CreateNewMvlPlayable()
{
	uint32_t newStructId = m_mvlHead->playables_count;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movelistBlock + m_mvlHead->playables_offset;

	ModifyGenericMovelistListSize<MvlPlayable>(newStructId, { -1 }, {}, listHead, DescribeRecordCount(m_mvlHead->playables_count));
	MvlPlayable* playable = m_iterators.mvl_playables[newStructId];
	playable->distance = 1000;
	SetupIterators_DisplayableMovelist();
	return newStructId;
}

uint32_t EditorT7::CreateNewMvlInputs()
{
	uint32_t newStructId = (uint32_t)m_iterators.mvl_inputs.size();
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movelistBlock + m_mvlHead->inputs_offset;

	ModifyGenericMovelistListSize<MvlInput>(newStructId, { -1, -1 }, {}, listHead, RecordListCount{m_iterators.mvl_inputs.size(), 0, 0});
	MvlInput* inputs = m_iterators.mvl_inputs[newStructId];
	inputs[0].frame_duration = 5;
	inputs[1].frame_duration = 5;

	m_iterators.mvl_inputs.set_size(newStructId + 2);
	return newStructId;
}