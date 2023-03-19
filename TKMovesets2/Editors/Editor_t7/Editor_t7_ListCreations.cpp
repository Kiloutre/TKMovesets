#include <map>
#include <format>

# include "Editor_t7.hpp"
# include "Helpers.hpp"

#define gAddr StructsT7_gameAddr
#define offsetof(st, m) ((size_t)(&((decltype(st) *)0)->m))

template<typename T>
void EditorT7::ModifyGenericListSize(int listId, int oldSize, int newSize, size_t tableListOffset)
{
	const int listSizeDiff = newSize - oldSize;
	const int structSize = sizeof(T);
	const int structListSize = structSize * newSize;
	const int structListSizeDiff = structSize * listSizeDiff;
	uint64_t tableListStart = *(uint64_t*)(((Byte*)&m_infos->table) + tableListOffset);

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t listOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock + tableListStart + listId * structSize;

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
	

	// Copy all the data up to the structure list 
	memcpy(newMoveset, m_moveset, listOffset);

	// Copy all the data after the structure list
	uint64_t postListOffset = listOffset + structSize * newSize;
	uint64_t orig_postListOffset = listOffset + structSize * oldSize;
	memcpy(newMoveset + postListOffset, m_moveset + orig_postListOffset, m_movesetSize - orig_postListOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	m_header->offsets.animationBlock += structListSizeDiff;
	m_header->offsets.motaBlock += structListSizeDiff;
}

void EditorT7::ModifyRequirementListSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<Requirement>(listId, oldSize, newSize, offsetof(m_infos->table, requirement));
	
	// Correct every structure that uses this list and needs shifting

	// Cancels
	for (auto& cancel : m_iterators.cancels)
	{
		if (cancel.requirements_addr >= listId + oldSize || (listSizeDiff < 0 && cancel.requirements_addr > listId)) {
			cancel.requirements_addr += listSizeDiff;
		}
	}

	// Grouped cancels
	for (auto& cancel : m_iterators.grouped_cancels)
	{
		if (cancel.requirements_addr >= listId + oldSize || (listSizeDiff < 0 && cancel.requirements_addr > listId)) {
			cancel.requirements_addr += listSizeDiff;
		}
	}

	// Hit conditions
	for (auto& hitCondition : m_iterators.hit_conditions)
	{
		if (hitCondition.requirements_addr >= listId + oldSize || (listSizeDiff < 0 && hitCondition.requirements_addr > listId)) {
			hitCondition.requirements_addr += listSizeDiff;
		}
	}

	// Move begin prop
	for (auto& otherProp : m_iterators.move_start_properties)
	{
		if (otherProp.requirements_addr >= listId + oldSize || (listSizeDiff < 0 && otherProp.requirements_addr > listId)) {
			otherProp.requirements_addr += listSizeDiff;
		}
	}

	// Move end prop
	for (auto& otherProp : m_iterators.move_end_properties)
	{
		if (otherProp.requirements_addr >= listId + oldSize || (listSizeDiff < 0 && otherProp.requirements_addr > listId)) {
			otherProp.requirements_addr += listSizeDiff;
		}
	}
}

void EditorT7::ModifyHitConditionListSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<HitCondition>(listId, oldSize, newSize, offsetof(m_infos->table, hitCondition));

	// Correct every structure that uses this list and needs shifting

	// Moves
	for (auto& move : m_iterators.moves)
	{
		if (move.hit_condition_addr != MOVESET_ADDR_MISSING) {
			if (move.hit_condition_addr >= listId + oldSize || (listSizeDiff < 0 && move.hit_condition_addr > listId)) {
				move.hit_condition_addr += listSizeDiff;
			}
		}
	}
	// Projectiles
	for (auto& projectile : m_iterators.projectiles)
	{
		if (projectile.hit_condition_addr >= listId + oldSize || (listSizeDiff < 0 && projectile.hit_condition_addr > listId)) {
			projectile.hit_condition_addr += listSizeDiff;
		}
	}
}

void EditorT7::ModifyGroupedCancelListSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<Cancel>(listId, oldSize, newSize, offsetof(m_infos->table, groupCancel));

	// Correct every structure that uses this list and needs shifting

	for (auto& cancel : m_iterators.cancels)
	{
		if (cancel.command == constants[EditorConstants_GroupedCancelCommand]) {
			if (cancel.move_id >= listId + oldSize || (listSizeDiff < 0 && cancel.move_id > listId)) {
				cancel.move_id += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyCancelListSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<Cancel>(listId, oldSize, newSize, offsetof(m_infos->table, cancel));

	// Correct every structure that uses this list and needs shifting

	for (auto& move : m_iterators.moves)
	{
		if (move.cancel_addr != MOVESET_ADDR_MISSING) {
			if (move.cancel_addr >= listId + oldSize || (listSizeDiff < 0 && move.cancel_addr > listId)) {
				move.cancel_addr += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyExtraPropertyListSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<ExtraMoveProperty>(listId, oldSize, newSize, offsetof(m_infos->table, extraMoveProperty));

	// Correct every structure that uses this list and needs shifting

	for (auto& move : m_iterators.moves)
	{
		if (move.extra_move_property_addr != MOVESET_ADDR_MISSING) {
			if (move.extra_move_property_addr >= listId + oldSize || (listSizeDiff < 0 && move.extra_move_property_addr > listId)) {
				move.extra_move_property_addr += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyStartPropertyListSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<OtherMoveProperty>(listId, oldSize, newSize, offsetof(m_infos->table, moveBeginningProp));

	// Correct every structure that uses this list and needs shifting

	for (auto& move : m_iterators.moves)
	{
		if (move.move_start_extraprop_addr != MOVESET_ADDR_MISSING) {
			if (move.move_start_extraprop_addr >= listId + oldSize || (listSizeDiff < 0 && move.move_start_extraprop_addr > listId)) {
				move.move_start_extraprop_addr += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyEndPropertyListSize(int listId, int oldSize, int newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<OtherMoveProperty>(listId, oldSize, newSize, offsetof(m_infos->table, moveEndingProp));

	// Correct every structure that uses this list and needs shifting

	for (auto& move : m_iterators.moves)
	{
		if (move.move_end_extraprop_addr != MOVESET_ADDR_MISSING) {
			if (move.move_end_extraprop_addr >= listId + oldSize || (listSizeDiff < 0 && move.move_end_extraprop_addr > listId)) {
				move.move_end_extraprop_addr += listSizeDiff;
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
	}
}