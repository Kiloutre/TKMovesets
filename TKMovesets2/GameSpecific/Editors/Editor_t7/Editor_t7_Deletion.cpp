#include "Animations.hpp"
# include "Editor_t7.hpp"
# include "Helpers.hpp"

using namespace StructsT7;
#define gAddr StructsT7_gameAddr

void EditorT7::DeleteMove(int id)
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


void EditorT7::DeleteCancelExtradata(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.cancelExtradata;
	RecordEdit edit(id, {}, {id}, m_infos->table.cancelExtradataCount);

	ModifyGenericMovelistListSize<CancelExtradata>(id, {}, { id }, listHead, DescribeRecordCount(m_infos->table.cancelExtradataCount));

	for (auto& cancel : m_iterators.cancels)
	{
		RemapRecordIndex(cancel.extradata_addr, edit);
	}
}

void EditorT7::DeleteReactions(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.reactions;
	RecordEdit edit(id, {}, {id}, m_infos->table.reactionsCount);

	ModifyGenericMovelistListSize<Reactions>(id, {}, { id }, listHead, DescribeRecordCount(m_infos->table.reactionsCount));

	for (auto& hitCondition : m_iterators.hit_conditions)
	{
		RemapRecordIndex(hitCondition.reactions_addr, edit);
	}
}

void EditorT7::DeletePushback(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.pushback;
	RecordEdit edit(id, {}, {id}, m_infos->table.pushbackCount);

	ModifyGenericMovelistListSize<Pushback>(id, {}, { id }, listHead, DescribeRecordCount(m_infos->table.pushbackCount));

	for (auto& reactions : m_iterators.reactions)
	{
		// Same remap as the other deletions: a reaction pointing at the deleted pushback moves to the next one
		for (auto& pushback : reactions.pushbacks) {
			RemapRecordIndex(pushback, edit);
		}
	}
	// No extra property references a pushback (docs/t7-structures.md, reference summary)
}

void EditorT7::DeleteInputSequence(int id)
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

void EditorT7::DeleteProjectile(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.projectile;
	RecordEdit edit(id, {}, {id}, m_infos->table.projectileCount);

	ModifyGenericMovelistListSize<Projectile>(id, {}, { id }, listHead, DescribeRecordCount(m_infos->table.projectileCount));

	for (auto& extraProp : m_iterators.extra_move_properties)
	{
		if (IsPropertyProjectileRef(extraProp.id)) {
			RemapRecordIndex(extraProp.value_unsigned, edit);
		}
	}
}

void EditorT7::DeleteThrowCamera(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.throwCameras;
	RecordEdit edit(id, {}, {id}, m_infos->table.throwCamerasCount);

	ModifyGenericMovelistListSize<ThrowCamera>(id, {}, { id }, listHead, DescribeRecordCount(m_infos->table.throwCamerasCount));

	for (auto& extraProp : m_iterators.extra_move_properties)
	{
		if (IsPropertyThrowCameraRef(extraProp.id)) {
			RemapRecordIndex(extraProp.value_unsigned, edit);
		}
	}
}

void EditorT7::DeleteCameraData(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.cameraData;
	RecordEdit edit(id, {}, {id}, m_infos->table.cameraDataCount);

	ModifyGenericMovelistListSize<CameraData>(id, {}, { id }, listHead, DescribeRecordCount(m_infos->table.cameraDataCount));

	for (auto& throwData : m_iterators.throw_datas)
	{
		RemapRecordIndex(throwData.cameradata_addr, edit);
	}
}

void EditorT7::DeleteMovelistPlayable(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movelistBlock + (uint64_t)m_mvlHead->playables_offset;
	RecordEdit edit(id, {}, {id}, m_mvlHead->playables_count);

	ModifyGenericMovelistListSize<MvlPlayable>(id, {}, { id }, listHead, DescribeRecordCount(m_mvlHead->playables_count));

	for (auto& displayable : m_iterators.mvl_displayables)
	{
		RemapRecordIndex(displayable.playable_id, edit);
	}
}

void EditorT7::DeleteStructures(EditorWindowType type, uint32_t id, size_t listSize)
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

void EditorT7::DeleteAnimation(uint64_t offset)
{
    Mutate([&] {
        const auto blockSize = m_offsets->motaBlock - m_offsets->animationBlock;
        if (offset >= blockSize) throw std::out_of_range("Animation deletion outside its section");
        auto end = blockSize;
        for (const auto& move : m_iterators.moves) {
            if (move.anim_addr == offset) throw std::invalid_argument("Animation is still referenced");
            if (move.anim_addr > offset) end = (std::min)(end, move.anim_addr);
        }
        for (const auto& [start, name] : m_animOffsetToNameOffset) {
            if (start >= blockSize) throw std::out_of_range("Animation offset outside its section");
            if (start > offset) end = (std::min)(end, start);
            if (start < offset) {
                auto data = m_movesetData + m_offsets->animationBlock + start;
                const auto size = TAnimUtils::FromMemory::Safe::GetAnimSize(data, blockSize - start);
                if (size > offset - start) throw std::invalid_argument("Deletion cuts through an animation");
            }
        }
        const auto removed = end - offset;
        ReplaceBlockRange(TKMovesetHeaderBlocks_Animation, offset, removed, {});
        for (auto& move : m_iterators.moves) if (move.anim_addr > offset) move.anim_addr -= removed;
        RebuildAnimationMaps();
    });
}

void EditorT7::DeleteNameBlockString(uint64_t offset)
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
        RebuildAnimationMaps();
    });
}
