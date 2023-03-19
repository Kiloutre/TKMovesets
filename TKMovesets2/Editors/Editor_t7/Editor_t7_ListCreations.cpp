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

	// Update table list address after re-allocation and also update the count 
	uint64_t* tableListCount = (uint64_t*)(((Byte*)&m_infos->table) + tableListOffset + 8);
	tableListStart = *(uint64_t*)(((Byte*)&m_infos->table) + tableListOffset);
	*tableListCount += listSizeDiff;

	uint64_t* countOffset = (uint64_t*)&m_infos->table;
	for (size_t i = 0; i < sizeof(MovesetTable) / 8 / 2; ++i)
	{
		if (*countOffset > tableListStart) {
			*countOffset += structListSizeDiff;
		}
		countOffset += 2;
	}
}

void EditorT7::ModifyRequirementListSize(uint32_t listId, uint16_t oldSize, uint16_t newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<Requirement>(listId, oldSize, newSize, offsetof(m_infos->table, requirement));
	
	// Correct every structure that uses this list and needs shifting
	{
		// Cancels
		uint64_t cancelListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.cancel;
		gAddr::Cancel* cancelPtr = (gAddr::Cancel*)(m_movesetData + cancelListOffset);

		uint16_t cancelId = 0;
		for (gAddr::Cancel* cancel = cancelPtr; cancelId < m_infos->table.cancelCount; ++cancelId, ++cancel)
		{
			if (cancel->requirements_addr >= listId + oldSize || (listSizeDiff < 0 && cancel->requirements_addr > listId)) {
				cancel->requirements_addr += listSizeDiff;
			}
		}
	}
	{
		// Grouped cancels
		uint64_t cancelListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.groupCancel;
		gAddr::Cancel* cancelPtr = (gAddr::Cancel*)(m_movesetData + cancelListOffset);

		uint16_t cancelId = 0;
		for (gAddr::Cancel* cancel = cancelPtr; cancelId < m_infos->table.groupCancelCount; ++cancelId, ++cancel)
		{
			if (cancel->requirements_addr >= listId + oldSize || (listSizeDiff < 0 && cancel->requirements_addr > listId)) {
				cancel->requirements_addr += listSizeDiff;
			}
		}
	}
	{
		// Hit conditions
		uint64_t hitConditionListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.hitCondition;
		gAddr::HitCondition* hitConditionPtr = (gAddr::HitCondition*)(m_movesetData + hitConditionListOffset);

		uint16_t hitConditionId = 0;
		for (gAddr::HitCondition* hitCondition = hitConditionPtr; hitConditionId < m_infos->table.hitConditionCount; ++hitConditionId, ++hitCondition)
		{
			if (hitCondition->requirements_addr >= listId + oldSize || (listSizeDiff < 0 && hitCondition->requirements_addr > listId)) {
				hitCondition->requirements_addr += listSizeDiff;
			}
		}
	}
	{
		// Move begin prop
		uint64_t otherPropListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.moveBeginningProp;
		gAddr::OtherMoveProperty* otherPropPtr = (gAddr::OtherMoveProperty*)(m_movesetData + otherPropListOffset);

		uint16_t propId = 0;
		for (gAddr::OtherMoveProperty* otherProp = otherPropPtr; propId < m_infos->table.moveBeginningPropCount; ++propId, ++otherProp)
		{
			if (otherPropPtr->requirements_addr >= listId + oldSize || (listSizeDiff < 0 && otherPropPtr->requirements_addr > listId)) {
				otherPropPtr->requirements_addr += listSizeDiff;
			}
		}
	}
	{
		// Move end prop
		uint64_t otherPropListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.moveEndingProp;
		gAddr::OtherMoveProperty* otherPropPtr = (gAddr::OtherMoveProperty*)(m_movesetData + otherPropListOffset);

		uint16_t propId = 0;
		for (gAddr::OtherMoveProperty* otherProp = otherPropPtr; propId < m_infos->table.moveEndingPropCount; ++propId, ++otherProp)
		{
			if (otherPropPtr->requirements_addr >= listId + oldSize || (listSizeDiff < 0 && otherPropPtr->requirements_addr > listId)) {
				otherPropPtr->requirements_addr += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyHitConditionListSize(uint32_t listId, uint16_t oldSize, uint16_t newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<HitCondition>(listId, oldSize, newSize, offsetof(m_infos->table, hitCondition));

	// Correct every structure that uses this list and needs shifting
	{
		// Moves
		uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
		gAddr::Move* movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);

		uint16_t moveId = 0;
		for (gAddr::Move* move = movePtr; moveId < m_infos->table.moveCount; ++moveId, ++move)
		{
			if (move->hit_condition_addr >= listId + oldSize || (listSizeDiff < 0 && move->hit_condition_addr > listId)) {
				move->hit_condition_addr += listSizeDiff;
			}
		}
	}
	{
		// Projectiles
		uint64_t projectileListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.projectile;
		gAddr::Projectile* projectilePtr = (gAddr::Projectile*)(m_movesetData + projectileListOffset);

		uint16_t projectileId = 0;
		for (gAddr::Projectile* projectile = projectilePtr; projectileId < m_infos->table.projectileCount; ++projectileId, ++projectile)
		{
			if (projectile->hit_condition_addr >= listId + oldSize || (listSizeDiff < 0 && projectile->hit_condition_addr > listId)) {
				projectile->hit_condition_addr += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyGroupedCancelListSize(uint32_t listId, uint16_t oldSize, uint16_t newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<Cancel>(listId, oldSize, newSize, offsetof(m_infos->table, groupCancel));

	// Correct every structure that uses this list and needs shifting
	{
		uint64_t cancelListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.cancel;
		gAddr::Cancel* cancelPtr = (gAddr::Cancel*)(m_movesetData + cancelListOffset);

		uint16_t cancelId = 0;
		for (gAddr::Cancel* cancel = cancelPtr; cancelId < m_infos->table.cancelCount; ++cancelId, ++cancel)
		{
			if (cancel->command == constants[EditorConstants_GroupedCancelCommand]) {
				if (cancel->move_id >= listId + oldSize || (listSizeDiff < 0 && cancel->move_id > listId)) {
					cancel->move_id += listSizeDiff;
				}
			}
		}
	}
}

void EditorT7::ModifyCancelListSize(uint32_t listId, uint16_t oldSize, uint16_t newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<Cancel>(listId, oldSize, newSize, offsetof(m_infos->table, cancel));

	// Correct every structure that uses this list and needs shifting
	{
		uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
		gAddr::Move* movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);

		uint16_t moveId = 0;
		for (gAddr::Move* move = movePtr; moveId < m_infos->table.moveCount; ++moveId, ++move)
		{
			if (move->cancel_addr >= listId + oldSize || (listSizeDiff < 0 && move->cancel_addr > listId)) {
				move->cancel_addr += listSizeDiff;
			}
		}
	}
}

void EditorT7::ModifyExtraPropertyListSize(uint32_t listId, uint16_t oldSize, uint16_t newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<ExtraMoveProperty>(listId, oldSize, newSize, offsetof(m_infos->table, extraMoveProperty));

	// Correct every structure that uses this list and needs shifting
	{
		uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
		gAddr::Move* movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);

		uint16_t moveId = 0;
		for (gAddr::Move* move = movePtr; moveId < m_infos->table.moveCount; ++moveId, ++move)
		{
			if (move->extra_move_property_addr != MOVESET_ADDR_MISSING) {
				if (move->extra_move_property_addr >= listId + oldSize || (listSizeDiff < 0 && move->extra_move_property_addr > listId)) {
					move->extra_move_property_addr += listSizeDiff;
				}
			}
		}
	}
}

void EditorT7::ModifyStartPropertyListSize(uint32_t listId, uint16_t oldSize, uint16_t newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<OtherMoveProperty>(listId, oldSize, newSize, offsetof(m_infos->table, moveBeginningProp));

	// Correct every structure that uses this list and needs shifting
	{
		uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
		gAddr::Move* movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);

		uint16_t moveId = 0;
		for (gAddr::Move* move = movePtr; moveId < m_infos->table.moveCount; ++moveId, ++move)
		{
			if (move->move_start_extraprop_addr != MOVESET_ADDR_MISSING) {
				if (move->move_start_extraprop_addr >= listId + oldSize || (listSizeDiff < 0 && move->move_start_extraprop_addr > listId)) {
					move->move_start_extraprop_addr += listSizeDiff;
				}
			}
		}
	}
}

void EditorT7::ModifyEndPropertyListSize(uint32_t listId, uint16_t oldSize, uint16_t newSize)
{
	const int listSizeDiff = newSize - oldSize;
	ModifyGenericListSize<OtherMoveProperty>(listId, oldSize, newSize, offsetof(m_infos->table, moveEndingProp));

	// Correct every structure that uses this list and needs shifting
	{
		uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
		gAddr::Move* movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);

		uint16_t moveId = 0;
		for (gAddr::Move* move = movePtr; moveId < m_infos->table.moveCount; ++moveId, ++move)
		{
			if (move->move_end_extraprop_addr != MOVESET_ADDR_MISSING) {
				if (move->move_end_extraprop_addr >= listId + oldSize || (listSizeDiff < 0 && move->move_end_extraprop_addr > listId)) {
					move->move_end_extraprop_addr += listSizeDiff;
				}
			}
		}
	}
}

void EditorT7::ModifyListSize(EditorWindowType_ type, uint32_t listId, uint16_t oldSize, uint16_t newSize)
{
	switch (type)
	{
	case EditorWindowType_Requirement:
		// todo: test
		ModifyRequirementListSize(listId, oldSize, newSize);
		break;

	case EditorWindowType_Cancel:
		ModifyCancelListSize(listId, oldSize, newSize);
		break;
	case EditorWindowType_GroupedCancel:
		// todo: test
		ModifyGroupedCancelListSize(listId, oldSize, newSize);
		break;

	case EditorWindowType_Extraproperty:
		// todo: test
		ModifyExtraPropertyListSize(listId, oldSize, newSize);
		break;
	case EditorWindowType_MoveBeginProperty:
		// todo: test
		ModifyStartPropertyListSize(listId, oldSize, newSize);
		break;
	case EditorWindowType_MoveEndProperty:
		// todo: test
		ModifyEndPropertyListSize(listId, oldSize, newSize);
		break;

	case EditorWindowType_HitCondition:
		// todo: test
		ModifyHitConditionListSize(listId, oldSize, newSize);
		break;
	}
}