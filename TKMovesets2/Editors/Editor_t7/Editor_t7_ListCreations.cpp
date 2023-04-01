# include "Editor_t7.hpp"
# include "Helpers.hpp"

#define gAddr StructsT7_gameAddr

template<typename T>
void EditorT7::ModifyGenericListSize(int listId, int oldSize, int newSize, size_t tableListOffset)
{
	const int listSizeDiff = newSize - oldSize;
	const uint64_t structSize = sizeof(T);
	const int structListSize = structSize * newSize;
	const int structListSizeDiff = structSize * listSizeDiff;
	uint64_t tableListStart = *(uint64_t*)(((Byte*)&m_infos->table) + tableListOffset);

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t listOffset = m_header->infos.header_size + m_header->offsets.movesetBlock + tableListStart + (uint64_t)listId * structSize;

	uint64_t postListOffset = listOffset + structSize * newSize;
	uint64_t orig_postListOffset = listOffset + structSize * oldSize;

	// todo: maybe align to 8 bytes in case the struct size is divisible by 4 and not 8. This is to keep following blocks 8 bytes aligned.
	//newMovesetSize = m_movesetSize + Helpers::align8Bytes(structListSizeDiff);
	newMovesetSize = m_movesetSize + structListSizeDiff;
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return;
	}

	// Update count & table offsets right now so that iterators built from LoadMovesetPtr() are up to date
	{
		uint64_t* tableListCount = (uint64_t*)(((Byte*)&m_infos->table) + tableListOffset + 8);
		*tableListCount += listSizeDiff;
		tableListStart = *(uint64_t*)(((Byte*)&m_infos->table) + tableListOffset);

		uint64_t* listHeadPtr = (uint64_t*)&m_infos->table;
		for (size_t i = 0; i < sizeof(MovesetTable) / 8 / 2; ++i)
		{
			if (*listHeadPtr > tableListStart) {
				*listHeadPtr += structListSizeDiff;
			}
			listHeadPtr += 2;
		}
	}

	// Shift offsets in the moveset table & in our header
	for (int i = 0; i < _countof(m_header->offsets.blocks); ++i)
	{
		if ((m_header->infos.header_size + m_header->offsets.blocks[i]) >= orig_postListOffset) {
			m_header->offsets.blocks[i] += structListSizeDiff;
			DEBUG_LOG("Shifted moveset block %d by 0x%x\n", i, structListSizeDiff);
		}
	}

	// Copy all the data up to the structure list 
	memcpy(newMoveset, m_moveset, listOffset);

	// Copy all the data after the structure list
	memcpy(newMoveset + postListOffset, m_moveset + orig_postListOffset, m_movesetSize - orig_postListOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

}

void EditorT7::ModifyRequirementListSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<Requirement>(listId, oldSize, newSize, offsetofVar(m_infos->table, requirement));
	
	// Correct every structure that uses this list and needs shifting

	// Cancels
	for (auto& cancel : m_iterators.cancels)
	{
		if (MUST_SHIFT_ID(cancel.requirements_addr, listSizeDiff, listId, listId + oldSize)) {
			cancel.requirements_addr += listSizeDiff;
		}
	}

	// Grouped cancels
	for (auto& cancel : m_iterators.grouped_cancels)
	{
		if (MUST_SHIFT_ID(cancel.requirements_addr, listSizeDiff, listId, listId + oldSize)) {
			cancel.requirements_addr += listSizeDiff;
		}
	}

	// Hit conditions
	for (auto& hitCondition : m_iterators.hit_conditions)
	{
		if (MUST_SHIFT_ID(hitCondition.requirements_addr, listSizeDiff, listId, listId + oldSize)) {
			hitCondition.requirements_addr += listSizeDiff;
		}
	}

	// Move begin prop
	for (auto& otherProp : m_iterators.move_start_properties)
	{
		if (MUST_SHIFT_ID(otherProp.requirements_addr, listSizeDiff, listId, listId + oldSize)) {
			otherProp.requirements_addr += listSizeDiff;
		}
	}

	// Move end prop
	for (auto& otherProp : m_iterators.move_end_properties)
	{
		if (MUST_SHIFT_ID(otherProp.requirements_addr, listSizeDiff, listId, listId + oldSize)) {
			otherProp.requirements_addr += listSizeDiff;
		}
	}
}

void EditorT7::ModifyHitConditionListSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<HitCondition>(listId, oldSize, newSize, offsetofVar(m_infos->table, hitCondition));

	// Correct every structure that uses this list and needs shifting

	// Moves
	for (auto& move : m_iterators.moves)
	{
		if (move.hit_condition_addr != MOVESET_ADDR_MISSING) {
			if (MUST_SHIFT_ID(move.hit_condition_addr, listSizeDiff, listId, listId + oldSize)) {
				move.hit_condition_addr += listSizeDiff;
			}
		}
	}
	// Projectiles
	for (auto& projectile : m_iterators.projectiles)
	{
		if (MUST_SHIFT_ID(projectile.hit_condition_addr, listSizeDiff, listId, listId + oldSize)) {
			projectile.hit_condition_addr += listSizeDiff;
		}
	}
}

void EditorT7::ModifyInputListSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<Input>(listId, oldSize, newSize, offsetofVar(m_infos->table, input));

	// Correct every structure that uses this list and needs shifting

	// Input sequences
	for (auto& sequence : m_iterators.input_sequences)
	{
		if (MUST_SHIFT_ID(sequence.input_addr, listSizeDiff, listId, listId + oldSize)) {
			sequence.input_addr += listSizeDiff;
		}
		else if (sequence.input_addr >= listId && sequence.input_addr <= ((uint64_t)listId + oldSize)) {
			sequence.input_amount += listSizeDiff;
		}
	}
}

void EditorT7::ModifyPushbackExtraListSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<PushbackExtradata>(listId, oldSize, newSize, offsetofVar(m_infos->table, pushbackExtradata));

	// Correct every structure that uses this list and needs shifting

	// Input sequences
	for (auto& pushback : m_iterators.pushbacks)
	{
		if (MUST_SHIFT_ID(pushback.extradata_addr, listSizeDiff, listId, listId + oldSize)) {
			pushback.extradata_addr += listSizeDiff;
		}
		else if (pushback.extradata_addr >= listId && pushback.extradata_addr <= ((uint64_t)listId + oldSize)) {
			pushback.num_of_loops += listSizeDiff;
		}
	}
}

void EditorT7::ModifyGroupedCancelListSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<Cancel>(listId, oldSize, newSize, offsetofVar(m_infos->table, groupCancel));

	// Correct every structure that uses this list and needs shifting

	for (auto& cancel : m_iterators.cancels)
	{
		if (cancel.command == constants->at(EditorConstants_GroupedCancelCommand)) {
			if (MUST_SHIFT_ID(cancel.move_id, listSizeDiff, listId, listId + oldSize)) {
				cancel.move_id += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyCancelListSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<Cancel>(listId, oldSize, newSize, offsetofVar(m_infos->table, cancel));

	// Correct every structure that uses this list and needs shifting

	for (auto& move : m_iterators.moves)
	{
		if (move.cancel_addr != MOVESET_ADDR_MISSING) {
			if (MUST_SHIFT_ID(move.cancel_addr, listSizeDiff, listId, listId + oldSize)) {
				move.cancel_addr += listSizeDiff;
			}
		}
	}

	// Projectiles
	for (auto& projectile : m_iterators.projectiles)
	{
		if (MUST_SHIFT_ID(projectile.cancel_addr, listSizeDiff, listId, listId + oldSize)) {
			projectile.cancel_addr += listSizeDiff;
		}
	}
}

void EditorT7::ModifyExtraPropertyListSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<ExtraMoveProperty>(listId, oldSize, newSize, offsetofVar(m_infos->table, extraMoveProperty));

	// Correct every structure that uses this list and needs shifting

	for (auto& move : m_iterators.moves)
	{
		if (move.extra_move_property_addr != MOVESET_ADDR_MISSING) {
			if (MUST_SHIFT_ID(move.extra_move_property_addr, listSizeDiff, listId, listId + oldSize)) {
				move.extra_move_property_addr += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyStartPropertyListSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<OtherMoveProperty>(listId, oldSize, newSize, offsetofVar(m_infos->table, moveBeginningProp));

	// Correct every structure that uses this list and needs shifting

	for (auto& move : m_iterators.moves)
	{
		if (move.move_start_extraprop_addr != MOVESET_ADDR_MISSING) {
			if (MUST_SHIFT_ID(move.move_start_extraprop_addr, listSizeDiff, listId, listId + oldSize)) {
				move.move_start_extraprop_addr += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyEndPropertyListSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<OtherMoveProperty>(listId, oldSize, newSize, offsetofVar(m_infos->table, moveEndingProp));

	// Correct every structure that uses this list and needs shifting

	for (auto& move : m_iterators.moves)
	{
		if (move.move_end_extraprop_addr != MOVESET_ADDR_MISSING) {
			if (MUST_SHIFT_ID(move.move_end_extraprop_addr, listSizeDiff, listId, listId + oldSize)) {
				move.move_end_extraprop_addr += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyVoiceclipListSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<Voiceclip>(listId, oldSize, newSize, offsetofVar(m_infos->table, voiceclip));

	// Correct every structure that uses this list and needs shifting

	for (auto& move : m_iterators.moves)
	{
		if (move.voicelip_addr != MOVESET_ADDR_MISSING) {
			if (MUST_SHIFT_ID(move.voicelip_addr, listSizeDiff, listId, listId + oldSize)) {
				move.voicelip_addr += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyListSize(EditorWindowType_ type, int listId, int oldSize, int newSize)
{
	switch (type)
	{
	case EditorWindowType_Requirement:
		ModifyRequirementListSize(listId, oldSize, newSize);
		break;

	case EditorWindowType_Cancel:
		ModifyCancelListSize(listId, oldSize, newSize);
		break;
	case EditorWindowType_GroupedCancel:
		ModifyGroupedCancelListSize(listId, oldSize, newSize);
		break;

	case EditorWindowType_Extraproperty:
		ModifyExtraPropertyListSize(listId, oldSize, newSize);
		break;
	case EditorWindowType_MoveBeginProperty:
		ModifyStartPropertyListSize(listId, oldSize, newSize);
		break;
	case EditorWindowType_MoveEndProperty:
		ModifyEndPropertyListSize(listId, oldSize, newSize);
		break;

	case EditorWindowType_HitCondition:
		ModifyHitConditionListSize(listId, oldSize, newSize);
		break;

	case EditorWindowType_Input:
		ModifyInputListSize(listId, oldSize, newSize);
		break;

	case EditorWindowType_PushbackExtradata:
		ModifyPushbackExtraListSize(listId, oldSize, newSize);
		break;

	case EditorWindowType_Voiceclip:
		ModifyVoiceclipListSize(listId, oldSize, newSize);
		break;
	}
}