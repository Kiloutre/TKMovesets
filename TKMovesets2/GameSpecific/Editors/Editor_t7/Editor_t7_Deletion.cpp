# include "Editor_t7.hpp"
# include "Helpers.hpp"

#define gAddr StructsT7_gameAddr

void EditorT7::DeleteMove(int id)
{
	/*
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.move;
	ModifyGenericMovelistListSize<Move>(id, {}, { id }, listHead);
	// delete move name
	// delete move anim

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
	ModifyGenericMovelistListSize<CancelExtradata>(id, {}, { id }, listHead);

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
	ModifyGenericMovelistListSize<Reactions>(id, {}, { id }, listHead);

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
	ModifyGenericMovelistListSize<Pushback>(id, {}, { id }, listHead);

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
	ModifyGenericMovelistListSize<InputSequence>(id, {}, { id }, listHead);

	for (auto& cancel : m_iterators.cancels)
	{
		if (IsCommandInputSequence(cancel.command)) {
			uint64_t correctedCommand = cancel.command - constants[EditorConstants_InputSequenceCommandStart];
			if (MUST_SHIFT_ID(correctedCommand, -1, id, id + 1)) {
				cancel.command -= 1;
			}
		}
	}
}

void EditorT7::DeleteProjectile(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.projectile;
	ModifyGenericMovelistListSize<Projectile>(id, {}, { id }, listHead);

	for (auto& extraProp : m_iterators.extra_move_properties)
	{
		if (IsPropertyProjectileRef(extraProp.id)) {
			if (MUST_SHIFT_ID(extraProp.value_unsigned, -1, id, id + 1)) {
				extraProp.value_unsigned -= 1;
			}
		}
	}
}

void EditorT7::DeleteThrowCamera(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.throwCameras;
	ModifyGenericMovelistListSize<ThrowCamera>(id, {}, { id }, listHead);

	for (auto& extraProp : m_iterators.extra_move_properties)
	{
		if (IsPropertyThrowCameraRef(extraProp.id)) {
			if (MUST_SHIFT_ID(extraProp.value_unsigned, -1, id, id + 1)) {
				extraProp.value_unsigned -= 1;
			}
		}
	}
}

void EditorT7::DeleteCameraData(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.cameraData;
	ModifyGenericMovelistListSize<CameraData>(id, {}, { id }, listHead);

	for (auto& throwData : m_iterators.throw_datas)
	{
		if (MUST_SHIFT_ID(throwData.cameradata_addr, -1, id, id + 1)) {
			throwData.cameradata_addr -= 1;
		}
	}
}

void EditorT7::DeleteMovelistPlayable(int id)
{
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movelistBlock + (uint64_t)m_mvlHead->playables_offset;
	ModifyGenericMovelistListSize<MvlPlayable>(id, {}, { id }, listHead);

	for (auto& displayable : m_iterators.mvl_displayables)
	{
		if (MUST_SHIFT_ID(displayable.playable_id, -1, id, id + 1))
		{
			displayable.playable_id -= 1;
		}
	}
}

void EditorT7::DeleteStructures(EditorWindowType type, uint32_t id, size_t listSize)
{
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
}

void EditorT7::DeleteAnimation(uint64_t anim_offset)
{
	DEBUG_LOG("DeleteAnimation\n");
	// MOTA block comes right after the animation block
	// If the animation is the last in its block, end addr shoudl be mota block
	uint64_t anim_end_addr = m_offsets->motaBlock;

	// Get animation end position
	for (const auto& move : m_iterators.moves) {
		if (anim_offset < move.anim_addr && move.anim_addr < anim_end_addr) {
			anim_end_addr = move.anim_addr;
		}
	}

	uint64_t anim_size = anim_end_addr - anim_offset;

	uint64_t newMovesetSize;
	Byte* newMoveset;

	const uint64_t listOffset = m_header->moveset_data_start + m_offsets->animationBlock + anim_offset;

	uint64_t postListOffset = listOffset;
	uint64_t orig_postListOffset = listOffset + anim_size;

	// Compute following m_offsets-> block position. We do this because we want to make sure it always stays 8 bytes aligned
	uint64_t new_followingBlockStart = m_movesetSize - anim_size; // Set initial value in case there is no following block
	uint64_t old_followingBlockStart = m_movesetSize; // Set initial value in case there is no following block
	for (unsigned int i = 0; i < m_header->block_list_size; ++i)
	{
		uint64_t absoluteBlockAddr = (m_header->moveset_data_start + m_offsets->blocks[i]);
		if (absoluteBlockAddr >= orig_postListOffset) {
			old_followingBlockStart = absoluteBlockAddr;
			new_followingBlockStart = Helpers::align8Bytes(old_followingBlockStart - anim_size);
			break;
		}
	}

	newMovesetSize = m_movesetSize + (new_followingBlockStart - old_followingBlockStart);
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		throw;
	}

	// Remove anim off map
	{
		auto tmp = m_animOffsetToNameOffset.find(anim_offset);
		if (tmp != m_animOffsetToNameOffset.end()) {
			m_animOffsetToNameOffset.erase(tmp);
		}
		else {
			// This should not happen
			DEBUG_ERR("Failed to find anim name in anim map. Most likely incorrect anim name.");
		}
	}

	// Shift blocks
	for (unsigned int i = 0; i < m_header->block_list_size; ++i)
	{
		if ((m_header->moveset_data_start + m_offsets->blocks[i]) > listOffset) {
			m_offsets->blocks[i] = Helpers::align8Bytes(m_offsets->blocks[i] - anim_size);
			DEBUG_LOG("Shifted moveset block %d by 0x%llx (before alignment)\n", i, anim_size);
		}
	}

	// Shift move anim addresses
	for (auto& m : m_iterators.moves)
	{
		if (m.anim_addr > anim_offset) {
			m.anim_addr -= anim_size;
		}
	}

	// Copy all the data up to the animation
	memcpy(newMoveset, m_moveset, listOffset);

	// Copy all the data after the animation, up to the start of the next block
	memcpy(newMoveset + postListOffset, m_moveset + orig_postListOffset, old_followingBlockStart - orig_postListOffset);

	// If there is a block afterward and not just the moveset end, copy everything from that block to the moveset end
	if (old_followingBlockStart != m_movesetSize) {
		memcpy(newMoveset + new_followingBlockStart, m_moveset + old_followingBlockStart, m_movesetSize - old_followingBlockStart);
	}

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);
}

void EditorT7::DeleteNameBlockString(uint64_t string_offset)
{
	DEBUG_LOG("DeleteNameBlockString\n");

	const char* name = (char*)(m_movesetData + m_offsets->nameBlock) + string_offset;
	uint64_t data_size = strlen(name) + 1;

	uint64_t newMovesetSize;
	Byte* newMoveset;

	const uint64_t data_absolute_offset = m_header->moveset_data_start + m_offsets->nameBlock + string_offset;

	uint64_t postDataOffset = data_absolute_offset;
	uint64_t orig_postDataOffset = data_absolute_offset + data_size;

	// Compute following m_offsets-> block position. We do this because we want to make sure it always stays 8 bytes aligned
	uint64_t new_followingBlockStart = m_movesetSize - data_size; // Set initial value in case there is no following block
	uint64_t old_followingBlockStart = m_movesetSize; // Set initial value in case there is no following block
	for (unsigned int i = 0; i < m_header->block_list_size; ++i)
	{
		uint64_t absoluteBlockAddr = (m_header->moveset_data_start + m_offsets->blocks[i]);
		if (absoluteBlockAddr >= orig_postDataOffset) {
			old_followingBlockStart = absoluteBlockAddr;
			new_followingBlockStart = Helpers::align8Bytes(old_followingBlockStart - data_size);
			break;
		}
	}

	newMovesetSize = m_movesetSize + (new_followingBlockStart - old_followingBlockStart);
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		throw;
	}

	// Remove name from map
	// Remove anim off map
	{
		auto tmp = m_animNameToOffsetMap.find(name);
		if (tmp != m_animNameToOffsetMap.end()) {
			m_animNameToOffsetMap.erase(tmp);
		}
		else {
			// This should not happen
			DEBUG_ERR("Failed to find anim name in anim map. Most likely incorrect anim name.");
		}
	}
	// Re-set map with new value if the same name exists multiple times in the name block (that can happen)
	for (auto& m : m_iterators.moves)
	{
		const char* m_name = (char*)(m_movesetData + m_offsets->nameBlock) + m.anim_name_addr;
		if (strcmp(m_name, name) == 0) {
			m_animNameToOffsetMap[m_name] = m.anim_addr;
			break;
		}
	}

	// Shift blocks
	for (unsigned int i = 0; i < m_header->block_list_size; ++i)
	{
		if ((m_header->moveset_data_start + m_offsets->blocks[i]) > data_absolute_offset) {
			m_offsets->blocks[i] = Helpers::align8Bytes(m_offsets->blocks[i] - data_size);
			DEBUG_LOG("Shifted moveset block %d by 0x%llx (before alignment)\n", i, data_size);
		}
	}

	// Shift move name & anim names
	for (auto& m : m_iterators.moves)
	{
		if (m.name_addr > string_offset) {
			m.name_addr -= data_size;
		}
		if (m.anim_name_addr > string_offset) {
			m.anim_name_addr -= data_size;
		}
	}

	// Re-adjust mappings
	for (auto& pair : m_animOffsetToNameOffset) {
		if (pair.second > string_offset) {
			pair.second -= data_size;
		}
	}

	// Copy all the data up to the animation
	memcpy(newMoveset, m_moveset, data_absolute_offset);

	// Copy all the data after the animation, up to the start of the next block
	memcpy(newMoveset + postDataOffset, m_moveset + orig_postDataOffset, old_followingBlockStart - orig_postDataOffset);

	// If there is a block afterward and not just the moveset end, copy everything from that block to the moveset end
	if (old_followingBlockStart != m_movesetSize) {
		memcpy(newMoveset + new_followingBlockStart, m_moveset + old_followingBlockStart, m_movesetSize - old_followingBlockStart);
	}

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);
}