# include "Editor_t7.hpp"
# include "Helpers.hpp"

using namespace StructsT7;
#define gAddr StructsT7_gameAddr

void EditorT7::ModifyRequirementListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.requirement;
	RecordEdit edit(listStart, ids, deletedIds, m_infos->table.requirementCount);

	ModifyGenericMovelistListSize<Requirement>(listStart, ids, deletedIds, listHead, DescribeRecordCount(m_infos->table.requirementCount));

	for (auto& cancel : m_iterators.cancels)
	{
		RemapRecordIndex(cancel.requirements_addr, edit);
	}

	// Grouped cancels
	for (auto& cancel : m_iterators.grouped_cancels)
	{
		RemapRecordIndex(cancel.requirements_addr, edit);
	}

	// Hit conditions
	for (auto& hitCondition : m_iterators.hit_conditions)
	{
		RemapRecordIndex(hitCondition.requirements_addr, edit);
	}

	// Move begin prop
	for (auto& otherProp : m_iterators.move_start_properties)
	{
		RemapRecordIndex(otherProp.requirements_addr, edit);
	}

	// Move end prop
	for (auto& otherProp : m_iterators.move_end_properties)
	{
		RemapRecordIndex(otherProp.requirements_addr, edit);
	}
}

void EditorT7::ModifyHitConditionListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.hitCondition;
	RecordEdit edit(listStart, ids, deletedIds, m_infos->table.hitConditionCount);

	ModifyGenericMovelistListSize<HitCondition>(listStart, ids, deletedIds, listHead, DescribeRecordCount(m_infos->table.hitConditionCount));

	for (auto& move : m_iterators.moves)
	{
		if (move.hit_condition_addr != MOVESET_ADDR_MISSING) {
			RemapRecordIndex(move.hit_condition_addr, edit);
		}
	}
	// Projectiles
	for (auto& projectile : m_iterators.projectiles)
	{
		RemapRecordIndex(projectile.hit_condition_addr, edit);
	}
}

void EditorT7::ModifyInputListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.input;
	RecordEdit edit(listStart, ids, deletedIds, m_infos->table.inputCount);

	ModifyGenericMovelistListSize<Input>(listStart, ids, deletedIds, listHead, DescribeRecordCount(m_infos->table.inputCount));


    for (auto& sequence : m_iterators.input_sequences) {
        auto [first, amount] = edit.RemapRange(sequence.input_addr, sequence.input_amount);
        if (amount > (std::numeric_limits<decltype(sequence.input_amount)>::max)())
            throw std::out_of_range("Referenced sequence is too long");
        sequence.input_addr = amount ? first : UINT64_MAX;
        sequence.input_amount = static_cast<decltype(sequence.input_amount)>(amount);
    }
}

void EditorT7::ModifyPushbackExtraListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.pushbackExtradata;
	RecordEdit edit(listStart, ids, deletedIds, m_infos->table.pushbackExtradataCount);

	ModifyGenericMovelistListSize<PushbackExtradata>(listStart, ids, deletedIds, listHead, DescribeRecordCount(m_infos->table.pushbackExtradataCount));


    for (auto& pushback : m_iterators.pushbacks) {
        auto [first, amount] = edit.RemapRange(pushback.extradata_addr, pushback.num_of_loops);
        if (amount > (std::numeric_limits<decltype(pushback.num_of_loops)>::max)())
            throw std::out_of_range("Referenced sequence is too long");
        pushback.extradata_addr = amount ? first : UINT64_MAX;
        pushback.num_of_loops = static_cast<decltype(pushback.num_of_loops)>(amount);
    }
}

void EditorT7::ModifyGroupedCancelListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.groupCancel;
	RecordEdit edit(listStart, ids, deletedIds, m_infos->table.groupCancelCount);

	ModifyGenericMovelistListSize<Cancel>(listStart, ids, deletedIds, listHead, DescribeRecordCount(m_infos->table.groupCancelCount));

	for (auto& cancel : m_iterators.cancels)
	{
		if (cancel.command == constants[EditorConstants_GroupedCancelCommand]) {
			RemapRecordIndex(cancel.move_id, edit);
		}
	}
}

void EditorT7::ModifyCancelListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.cancel;
	RecordEdit edit(listStart, ids, deletedIds, m_infos->table.cancelCount);

	ModifyGenericMovelistListSize<Cancel>(listStart, ids, deletedIds, listHead, DescribeRecordCount(m_infos->table.cancelCount));

	for (auto& move : m_iterators.moves)
	{
		        RemapRecordIndex(move.cancel_addr, edit);
        RemapRecordIndex(move._0x28_cancel_addr, edit);
        RemapRecordIndex(move._0x38_cancel_addr, edit);
        RemapRecordIndex(move._0x48_cancel_addr, edit);
	}

	// Projectiles
	for (auto& projectile : m_iterators.projectiles)
	{
		RemapRecordIndex(projectile.cancel_addr, edit);
	}
}

void EditorT7::ModifyExtraPropertyListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.extraMoveProperty;
	RecordEdit edit(listStart, ids, deletedIds, m_infos->table.extraMovePropertyCount);

	ModifyGenericMovelistListSize<ExtraMoveProperty>(listStart, ids, deletedIds, listHead, DescribeRecordCount(m_infos->table.extraMovePropertyCount));

	for (auto& move : m_iterators.moves)
	{
		if (move.extra_move_property_addr != MOVESET_ADDR_MISSING) {
			RemapRecordIndex(move.extra_move_property_addr, edit);
		}
	}
}

void EditorT7::ModifyStartPropertyListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.moveBeginningProp;
	RecordEdit edit(listStart, ids, deletedIds, m_infos->table.moveBeginningPropCount);

	ModifyGenericMovelistListSize<OtherMoveProperty>(listStart, ids, deletedIds, listHead, DescribeRecordCount(m_infos->table.moveBeginningPropCount));

	for (auto& move : m_iterators.moves)
	{
		if (move.move_start_extraprop_addr != MOVESET_ADDR_MISSING) {
			RemapRecordIndex(move.move_start_extraprop_addr, edit);
		}
	}
}

void EditorT7::ModifyEndPropertyListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.moveEndingProp;
	RecordEdit edit(listStart, ids, deletedIds, m_infos->table.moveEndingPropCount);

	ModifyGenericMovelistListSize<OtherMoveProperty>(listStart, ids, deletedIds, listHead, DescribeRecordCount(m_infos->table.moveEndingPropCount));

	for (auto& move : m_iterators.moves)
	{
		if (move.move_end_extraprop_addr != MOVESET_ADDR_MISSING) {
			RemapRecordIndex(move.move_end_extraprop_addr, edit);
		}
	}
}

void EditorT7::ModifyVoiceclipListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.voiceclip;
	RecordEdit edit(listStart, ids, deletedIds, m_infos->table.voiceclipCount);

	ModifyGenericMovelistListSize<Voiceclip>(listStart, ids, deletedIds, listHead, DescribeRecordCount(m_infos->table.voiceclipCount));

	for (auto& move : m_iterators.moves)
	{
		if (move.voicelip_addr != MOVESET_ADDR_MISSING) {
			RemapRecordIndex(move.voicelip_addr, edit);
		}
	}
}

void EditorT7::ModifyListSize(EditorWindowType type, unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	return Mutate([&]() {
    RecordEdit checked(listStart, ids, deletedIds, GetStructureCount(type));

	switch (type)
	{
	case TEditorWindowType_Requirement:
		ModifyRequirementListSize(listStart, ids, deletedIds);
		break;
	case TEditorWindowType_Extraproperty:
		ModifyExtraPropertyListSize(listStart, ids, deletedIds);
		break;

	case TEditorWindowType_GroupedCancel:
		ModifyGroupedCancelListSize(listStart, ids, deletedIds);
		break;

	case TEditorWindowType_MoveBeginProperty:
		ModifyStartPropertyListSize(listStart, ids, deletedIds);
		break;
	case TEditorWindowType_MoveEndProperty:
		ModifyEndPropertyListSize(listStart, ids, deletedIds);
		break;

	case TEditorWindowType_HitCondition:
		ModifyHitConditionListSize(listStart, ids, deletedIds);
		break;

	case TEditorWindowType_Input:
		ModifyInputListSize(listStart, ids, deletedIds);
		break;

	case TEditorWindowType_PushbackExtradata:
		ModifyPushbackExtraListSize(listStart, ids, deletedIds);
		break;

	case TEditorWindowType_Voiceclip:
		ModifyVoiceclipListSize(listStart, ids, deletedIds);
		break;

	case TEditorWindowType_Cancel:
		ModifyCancelListSize(listStart, ids, deletedIds);
		break;


	case TEditorWindowType_MovelistDisplayable:
		ModifyMovelistDisplayableSize(listStart, ids, deletedIds);
		break;
	case TEditorWindowType_MovelistInput:
		ModifyMovelistInputSize(listStart, ids, deletedIds);
		break;
	}

	});
}
