# include "Editor_t7.hpp"
# include "Helpers.hpp"

using namespace StructsT7;
#define gAddr StructsT7_gameAddr

void EditorT7::ModifyRequirementListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.requirement;
	int listSizeDiff = ModifyGenericMovelistListSize<Requirement>(listStart, ids, deletedIds, listHead);

	int oldSize = (int)ids.size() - listSizeDiff;
	for (auto& cancel : m_iterators.cancels)
	{
		if (MUST_SHIFT_ID(cancel.requirements_addr, listSizeDiff, listStart, listStart + oldSize)) {
			cancel.requirements_addr += listSizeDiff;
		}
	}

	// Grouped cancels
	for (auto& cancel : m_iterators.grouped_cancels)
	{
		if (MUST_SHIFT_ID(cancel.requirements_addr, listSizeDiff, listStart, listStart + oldSize)) {
			cancel.requirements_addr += listSizeDiff;
		}
	}

	// Hit conditions
	for (auto& hitCondition : m_iterators.hit_conditions)
	{
		if (MUST_SHIFT_ID(hitCondition.requirements_addr, listSizeDiff, listStart, listStart + oldSize)) {
			hitCondition.requirements_addr += listSizeDiff;
		}
	}

	// Move begin prop
	for (auto& otherProp : m_iterators.move_start_properties)
	{
		if (MUST_SHIFT_ID(otherProp.requirements_addr, listSizeDiff, listStart, listStart + oldSize)) {
			otherProp.requirements_addr += listSizeDiff;
		}
	}

	// Move end prop
	for (auto& otherProp : m_iterators.move_end_properties)
	{
		if (MUST_SHIFT_ID(otherProp.requirements_addr, listSizeDiff, listStart, listStart + oldSize)) {
			otherProp.requirements_addr += listSizeDiff;
		}
	}
}

void EditorT7::ModifyHitConditionListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.hitCondition;
	int listSizeDiff = ModifyGenericMovelistListSize<HitCondition>(listStart, ids, deletedIds, listHead);

	int oldSize = (int)ids.size() - listSizeDiff;
	for (auto& move : m_iterators.moves)
	{
		if (move.hit_condition_addr != MOVESET_ADDR_MISSING) {
			if (MUST_SHIFT_ID(move.hit_condition_addr, listSizeDiff, listStart, listStart + oldSize)) {
				move.hit_condition_addr += listSizeDiff;
			}
		}
	}
	// Projectiles
	for (auto& projectile : m_iterators.projectiles)
	{
		if (MUST_SHIFT_ID(projectile.hit_condition_addr, listSizeDiff, listStart, listStart + oldSize)) {
			projectile.hit_condition_addr += listSizeDiff;
		}
	}
}

void EditorT7::ModifyInputListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.input;
	int listSizeDiff = ModifyGenericMovelistListSize<Input>(listStart, ids, deletedIds, listHead);

	int oldSize = (int)ids.size() - listSizeDiff;
	// Input sequences
	for (auto& sequence : m_iterators.input_sequences)
	{
		if (MUST_SHIFT_ID(sequence.input_addr, listSizeDiff, listStart, listStart + oldSize)) {
			sequence.input_addr += listSizeDiff;
		}
		else if (sequence.input_addr >= listStart && sequence.input_addr <= ((uint64_t)listStart + oldSize)) {
			sequence.input_amount += listSizeDiff;
		}
	}
}

void EditorT7::ModifyPushbackExtraListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.pushbackExtradata;
	int listSizeDiff = ModifyGenericMovelistListSize<PushbackExtradata>(listStart, ids, deletedIds, listHead);

	int oldSize = (int)ids.size() - listSizeDiff;
	// Input sequences
	for (auto& pushback : m_iterators.pushbacks)
	{
		if (MUST_SHIFT_ID(pushback.extradata_addr, listSizeDiff, listStart, listStart + oldSize)) {
			pushback.extradata_addr += listSizeDiff;
		}
		else if (pushback.extradata_addr >= listStart && pushback.extradata_addr <= ((uint64_t)listStart + oldSize)) {
			pushback.num_of_loops += listSizeDiff;
		}
	}
}

void EditorT7::ModifyGroupedCancelListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.groupCancel;
	int listSizeDiff = ModifyGenericMovelistListSize<Cancel>(listStart, ids, deletedIds, listHead);

	int oldSize = (int)ids.size() - listSizeDiff;
	for (auto& cancel : m_iterators.cancels)
	{
		if (cancel.command == constants[EditorConstants_GroupedCancelCommand]) {
			if (MUST_SHIFT_ID(cancel.move_id, listSizeDiff, listStart, listStart + oldSize)) {
				cancel.move_id += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyCancelListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.cancel;
	int listSizeDiff = ModifyGenericMovelistListSize<Cancel>(listStart, ids, deletedIds, listHead);

	int oldSize = (int)ids.size() - listSizeDiff;
	for (auto& move : m_iterators.moves)
	{
		if (move.cancel_addr != MOVESET_ADDR_MISSING) {
			if (MUST_SHIFT_ID(move.cancel_addr, listSizeDiff, listStart, listStart + oldSize)) {
				move.cancel_addr += listSizeDiff;
			}
		}
	}

	// Projectiles
	for (auto& projectile : m_iterators.projectiles)
	{
		if (MUST_SHIFT_ID(projectile.cancel_addr, listSizeDiff, listStart, listStart + oldSize)) {
			projectile.cancel_addr += listSizeDiff;
		}
	}
}

void EditorT7::ModifyExtraPropertyListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.extraMoveProperty;
	int listSizeDiff = ModifyGenericMovelistListSize<ExtraMoveProperty>(listStart, ids, deletedIds, listHead);

	int oldSize = (int)ids.size() - listSizeDiff;
	for (auto& move : m_iterators.moves)
	{
		if (move.extra_move_property_addr != MOVESET_ADDR_MISSING) {
			if (MUST_SHIFT_ID(move.extra_move_property_addr, listSizeDiff, listStart, listStart + oldSize)) {
				move.extra_move_property_addr += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyStartPropertyListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.moveBeginningPropCount;
	int listSizeDiff = ModifyGenericMovelistListSize<OtherMoveProperty>(listStart, ids, deletedIds, listHead);

	int oldSize = (int)ids.size() - listSizeDiff;
	for (auto& move : m_iterators.moves)
	{
		if (move.move_start_extraprop_addr != MOVESET_ADDR_MISSING) {
			if (MUST_SHIFT_ID(move.move_start_extraprop_addr, listSizeDiff, listStart, listStart + oldSize)) {
				move.move_start_extraprop_addr += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyEndPropertyListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.moveEndingProp;
	int listSizeDiff = ModifyGenericMovelistListSize<OtherMoveProperty>(listStart, ids, deletedIds, listHead);

	int oldSize = (int)ids.size() - listSizeDiff;
	for (auto& move : m_iterators.moves)
	{
		if (move.move_end_extraprop_addr != MOVESET_ADDR_MISSING) {
			if (MUST_SHIFT_ID(move.move_end_extraprop_addr, listSizeDiff, listStart, listStart + oldSize)) {
				move.move_end_extraprop_addr += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyVoiceclipListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.voiceclip;
	int listSizeDiff = ModifyGenericMovelistListSize<Voiceclip>(listStart, ids, deletedIds, listHead);

	int oldSize = (int)ids.size() - listSizeDiff;
	for (auto& move : m_iterators.moves)
	{
		if (move.voicelip_addr != MOVESET_ADDR_MISSING) {
			if (MUST_SHIFT_ID(move.voicelip_addr, listSizeDiff, listStart, listStart + oldSize)) {
				move.voicelip_addr += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyListSize(EditorWindowType type, unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds)
{
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
}