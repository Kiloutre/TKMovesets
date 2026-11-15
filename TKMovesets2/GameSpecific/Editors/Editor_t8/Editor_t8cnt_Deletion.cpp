# include "Editor_t8cnt.hpp"
# include "Helpers.hpp"

using namespace StructsT8CNT;

#define gAddr StructsT8CNT_gameAddr

void EditorT8CNT::DeleteMove(int id)
{
	/*
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.move;
	RecordEdit edit(id, {}, {id}, m_infos->table.moveCount);

	ModifyGenericMovelistListSize<Move>(id, {}, { id }, listHead, DescribeRecordCount(m_infos->table.moveCount));
	// delete move name
	// delete move anim

	for (auto& cancel : m_iterators.cancels)
	{
		RemapRecordIndex(cancel.move_id, edit);
	}

	for (auto& reactions : m_iterators.reactions)
	{
		for (unsigned int i = 0; i < _countof(reactions.moveids); ++i) {
			RemapRecordIndex(reactions.moveids[i], edit);
		}
	}

	for (auto& extraProp : m_iterators.extra_move_properties)
	{
		// Probably not a idea to allow move deletion at all unless it's a custom one, because of extra prop referring to moves
	}
	*/
}


void EditorT8CNT::DeleteCancelExtradata(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.cancelExtradata;
	RecordEdit edit(id, {}, {id}, m_infos->table.cancelExtradataCount);

	ModifyGenericMovelistListSize<CancelExtradata>(id, {}, { id }, listHead, DescribeRecordCount(m_infos->table.cancelExtradataCount));

	for (auto& cancel : m_iterators.cancels)
	{
		RemapRecordIndex(cancel.extradata_addr, edit);
	}
}

void EditorT8CNT::DeleteReactions(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.reactions;
	RecordEdit edit(id, {}, {id}, m_infos->table.reactionsCount);

	ModifyGenericMovelistListSize<Reactions>(id, {}, { id }, listHead, DescribeRecordCount(m_infos->table.reactionsCount));

	for (auto& hitCondition : m_iterators.hit_conditions)
	{
		RemapRecordIndex(hitCondition.reactions_addr, edit);
	}
}

void EditorT8CNT::DeletePushback(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.pushback;
	RecordEdit edit(id, {}, {id}, m_infos->table.pushbackCount);

	ModifyGenericMovelistListSize<Pushback>(id, {}, { id }, listHead, DescribeRecordCount(m_infos->table.pushbackCount));

	for (auto& reactions : m_iterators.reactions)
	{
		for (unsigned int i = 0; i < _countof(reactions.pushbacks); ++i) {
			if (MUST_SHIFT_ID(reactions.pushbacks[i], -1, id, id + 1)) {
				reactions.pushbacks[i]  -= 1;
			}
		}
	}


	for (auto& extraProp : m_iterators.extra_move_properties)
	{
		//TODO
	}
}

void EditorT8CNT::DeleteInputSequence(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.inputSequence;
	RecordEdit edit(id, {}, {id}, m_infos->table.inputSequenceCount);

	ModifyGenericMovelistListSize<InputSequence>(id, {}, { id }, listHead, DescribeRecordCount(m_infos->table.inputSequenceCount));

	for (auto& cancel : m_iterators.cancels)
	{
		if (IsCommandInputSequence(cancel.command)) {
            const uint64_t base = constants.at(EditorConstants_InputSequenceCommandStart);
            auto index = static_cast<uint32_t>(cancel.command) - base;
            auto mapped = edit.Remap(index);
            if (mapped == UINT64_MAX || mapped >= 0x10000 - base)
                throw std::invalid_argument("Cannot remove the last input sequence while a cancel references it");
            cancel.command = (cancel.command & 0xffffffff00000000ULL) | (base + mapped);
		}
	}
}

void EditorT8CNT::DeleteProjectile(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.projectile;
	RecordEdit edit(id, {}, {id}, m_infos->table.projectileCount);

	ModifyGenericMovelistListSize<Projectile>(id, {}, { id }, listHead, DescribeRecordCount(m_infos->table.projectileCount));

	for (auto& extraProp : m_iterators.extra_move_properties)
	{
		if (IsPropertyProjectileRef(extraProp.id)) {
			/*
			RemapRecordIndex(extraProp.value_unsigned, edit);
			*/
		}
	}
}

void EditorT8CNT::DeleteThrowCamera(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.throwCameras;
	RecordEdit edit(id, {}, {id}, m_infos->table.throwCamerasCount);

	ModifyGenericMovelistListSize<ThrowCamera>(id, {}, { id }, listHead, DescribeRecordCount(m_infos->table.throwCamerasCount));

	for (auto& extraProp : m_iterators.extra_move_properties)
	{
		if (IsPropertyThrowCameraRef(extraProp.id)) {
			/*
			RemapRecordIndex(extraProp.value_unsigned, edit);
			*/
		}
	}
}

void EditorT8CNT::DeleteCameraData(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.cameraData;
	RecordEdit edit(id, {}, {id}, m_infos->table.cameraDataCount);

	ModifyGenericMovelistListSize<CameraData>(id, {}, { id }, listHead, DescribeRecordCount(m_infos->table.cameraDataCount));

	for (auto& throwData : m_iterators.throw_datas)
	{
		RemapRecordIndex(throwData.cameradata_addr, edit);
	}
}

void EditorT8CNT::DeleteMovelistPlayable(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movelistBlock + (uint64_t)m_mvlHead->playables_offset;
	RecordEdit edit(id, {}, {id}, m_mvlHead->playables_count);

	ModifyGenericMovelistListSize<MvlPlayable>(id, {}, { id }, listHead, DescribeRecordCount(m_mvlHead->playables_count));

	for (auto& displayable : m_iterators.mvl_displayables)
	{
		RemapRecordIndex(displayable.playable_id, edit);
	}
}

void EditorT8CNT::DeleteStructures(EditorWindowType type, uint32_t id, size_t listSize)
{
	return Mutate([&]() {
    const auto total = GetStructureCount(type);
    const auto amount = listSize ? listSize : 1;
    if (id >= total || amount > total - id) throw std::out_of_range("Deletion exceeds the record list");

	if (id > INT_MAX || listSize > INT_MAX || listSize > static_cast<size_t>(INT_MAX) - id) throw std::out_of_range("Deletion range is too large");

	if (listSize == 0) {
		listSize = 1;
	}
	std::set<int> itemsToDelete;

	for (unsigned int i = 0; i < listSize; ++i) {
		itemsToDelete.insert(id + i);
	}

	switch (type)
	{
		// Structure lists
	case TEditorWindowType_Requirement:
		ModifyRequirementListSize(id, {}, itemsToDelete);
		break;
	case TEditorWindowType_Voiceclip:
		ModifyVoiceclipListSize(id, {}, itemsToDelete);
		break;

	case TEditorWindowType_Cancel:
		ModifyCancelListSize(id, {}, itemsToDelete);
		break;
	case TEditorWindowType_GroupedCancel:
		ModifyGroupedCancelListSize(id, {}, itemsToDelete);
		break;

	case TEditorWindowType_Extraproperty:
		ModifyExtraPropertyListSize(id, {}, itemsToDelete);
		break;
	case TEditorWindowType_MoveBeginProperty:
		ModifyStartPropertyListSize(id, {}, itemsToDelete);
		break;
	case TEditorWindowType_MoveEndProperty:
		ModifyEndPropertyListSize(id, {}, itemsToDelete);
		break;

	case TEditorWindowType_HitCondition:
		ModifyHitConditionListSize(id, {}, itemsToDelete);
		break;
	case TEditorWindowType_PushbackExtradata:
		ModifyPushbackExtraListSize(id, {}, itemsToDelete);
		break;

	case TEditorWindowType_Input:
		ModifyInputListSize(id, {}, itemsToDelete);
		break;

	case TEditorWindowType_MovelistInput:
		ModifyMovelistInputSize(id, {}, itemsToDelete);
		break;

		// Structures
	case TEditorWindowType_Move:
		DeleteMove(id);
		break;
	case TEditorWindowType_CancelExtradata:
		DeleteCancelExtradata(id);
		break;

	case TEditorWindowType_Reactions:
		DeleteReactions(id);
		break;
	case TEditorWindowType_Pushback:
		DeletePushback(id);
		break;

	case TEditorWindowType_InputSequence:
		DeleteInputSequence(id);
		break;

	case TEditorWindowType_Projectile:
		DeleteProjectile(id);
		break;

	case TEditorWindowType_ThrowCamera:
		DeleteThrowCamera(id);
		break;
	case TEditorWindowType_CameraData:
		DeleteCameraData(id);
		break;


	case TEditorWindowType_MovelistPlayable:
		DeleteMovelistPlayable(id);
		break;

	}

	});
}

void EditorT8CNT::DeleteAnimation(uint64_t)
{
    throw std::logic_error("T8 encrypted animation references cannot be edited yet");
}

void EditorT8CNT::DeleteNameBlockString(uint64_t offset)
{
    Mutate([&] {
        const auto removed = BlockString(TKMovesetHeaderBlocks_Name, offset).size() + 1;
        if (NameRangeReferenced(offset, removed)) throw std::invalid_argument("Name is still referenced");
        const auto oldEnd = m_offsets->movesetBlock;
        ReplaceBlockRange(TKMovesetHeaderBlocks_Name, offset, removed, {});
        for (auto& move : m_iterators.moves) {
            if (move.name_addr > offset) move.name_addr -= removed;
            if (move.anim_name_addr > offset) move.anim_name_addr -= removed;
        }
        for (auto field : {&m_infos->character_name_addr, &m_infos->character_creator_addr, &m_infos->date_addr, &m_infos->fulldate_addr})
            if (auto value = reinterpret_cast<uint64_t>(*field);
                value > m_offsets->nameBlock + offset && value < oldEnd)
                *field = reinterpret_cast<char*>(value - removed);
        for (auto& [animation, name] : m_animOffsetToNameOffset)
            if (name > offset) name -= removed;
    });
}
