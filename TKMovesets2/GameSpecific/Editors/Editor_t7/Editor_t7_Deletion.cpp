# include "Editor_t7.hpp"
# include "Helpers.hpp"

#define gAddr StructsT7_gameAddr

void EditorT7::DeleteMove(int id)
{
	/*
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.move;
	ModifyGenericMovelistListSize<Move>(id, {}, itemsToDelete, listHead);

	for (auto& cancel : m_iterators.cancels)
	{
		if (MUST_SHIFT_ID(cancel.move_id, -1, id, id + 1))
		{
			cancel.move_id -= 1;
		}
	}

	for (auto& reactions : m_iterators.reactions)
	{
		for (unsigned int i = 0; i < _countof(reactions.moveids); ++i) {
			if (MUST_SHIFT_ID(reactions.moveids[i], -1, id, id + 1)) {
				reactions.moveids[i] -= 1;
			}
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
	ModifyGenericMovelistListSize<CancelExtradata>(id, {}, itemsToDelete, listHead);

	for (auto& cancel : m_iterators.cancels)
	{
		if (MUST_SHIFT_ID(cancel.extradata_addr, -1, id, id + 1))
		{
			cancel.extradata_addr -= 1;
		}
	}
}

void EditorT7::DeleteReactions(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.reactions;
	ModifyGenericMovelistListSize<Reactions>(id, {}, itemsToDelete, listHead);

	for (auto& hitCondition : m_iterators.hit_conditions)
	{
		if (MUST_SHIFT_ID(hitCondition.reactions_addr, -1, id, id + 1))
		{
			hitCondition.reactions_addr -= 1;
		}
	}
}

void EditorT7::DeletePushback(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.pushback;
	ModifyGenericMovelistListSize<Pushback>(id, {}, itemsToDelete, listHead);

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

void EditorT7::DeleteInputSequence(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.inputSequence;
	ModifyGenericMovelistListSize<InputSequence>(id, {}, itemsToDelete, listHead);

	for (auto& cancel : m_iterators.cancels)
	{
		if (IsCommandInputSequence(cancel.command)) {
			if (MUST_SHIFT_ID(cancel.command - constants->at(EditorConstants_InputSequenceCommandStart), -1, id, id + 1)) {
				cancel.command -= 1;
			}
		}
	}
}

void EditorT7::DeleteProjectile(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.projectile;
	ModifyGenericMovelistListSize<Projectile>(id, {}, itemsToDelete, listHead);

	for (auto& extraProp : m_iterators.extra_move_properties)
	{
		if (IsPropertyProjectileRef(extraProp.id)) {
			if (MUST_SHIFT_ID(extraProp.value, -1, id, id + 1)) {
				extraProp.value -= 1;
			}
		}
	}
}

void EditorT7::DeleteThrowCamera(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.throwCameras;
	ModifyGenericMovelistListSize<ThrowCamera>(id, {}, itemsToDelete, listHead);

	for (auto& extraProp : m_iterators.extra_move_properties)
	{
		if (IsPropertyThrowCameraRef(extraProp.id)) {
			if (MUST_SHIFT_ID(extraProp.value, -1, id, id + 1)) {
				extraProp.value -= 1;
			}
		}
	}
}

void EditorT7::DeleteCameraData(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.cameraData;
	ModifyGenericMovelistListSize<CameraData>(id, {}, itemsToDelete, listHead);

	for (auto& throwData : m_iterators.throw_datas)
	{
		if (MUST_SHIFT_ID(throwData.cameradata_addr, -1, id, id + 1)) {
			throwData.cameradata_addr -= 1;
		}
	}
}

void EditorT7::DeleteMovelistPlayable(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movelistBlock + (uint64_t)m_infos->table.playables_offset;
	ModifyGenericMovelistListSize<MvlPlayable>(id, {}, itemsToDelete, listHead);

	for (auto& displayable : m_iterators.mvl_displayables)
	{
		if (MUST_SHIFT_ID(displayable.playable_id, -1, id, id + 1))
		{
			displayable.playable_id -= 1;
		}
	}
}