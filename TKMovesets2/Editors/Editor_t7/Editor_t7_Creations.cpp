#include <map>
#include <format>

# include "Editor_t7.hpp"
# include "Helpers.hpp"

#define gAddr StructsT7_gameAddr

int32_t EditorT7::CreateNewExtraProperties()
{
	const uint16_t newStructId = m_infos->table.extraMovePropertyCount;

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t newStructOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock + (uint64_t)m_infos->table.extraMoveProperty + newStructId * sizeof(ExtraMoveProperty);

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = m_movesetSize + sizeof(ExtraMoveProperty) * 2;
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return -1;
	}

	// Copy all the data up to the new structure 
	memcpy(newMoveset, m_moveset, newStructOffset);

	// Initialize our structure value
	ExtraMoveProperty prop{ 0 };
	ExtraMoveProperty prop2{ 0 };

	prop.starting_frame = 32769;

	// Write our new structure
	memcpy(newMoveset + newStructOffset, &prop, sizeof(ExtraMoveProperty));
	memcpy(newMoveset + newStructOffset + sizeof(ExtraMoveProperty), &prop2, sizeof(ExtraMoveProperty));

	// Copy all the data after new the new structure
	uint64_t newStructPostOffset = newStructOffset + sizeof(ExtraMoveProperty) * 2;
	memcpy(newMoveset + newStructPostOffset, m_moveset + newStructOffset, m_movesetSize - newStructOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	const uint64_t extraSize = sizeof(ExtraMoveProperty) * 2;
	m_header->offsets.animationBlock += extraSize;
	m_header->offsets.motaBlock += extraSize;
	m_infos->table.extraMovePropertyCount += 2;

	// Increment moveset block offsets
	uint64_t* countOffset = (uint64_t*)&m_infos->table;
	for (size_t i = 0; i < sizeof(MovesetTable) / 8 / 2; ++i)
	{
		if (*countOffset > (uint64_t)m_infos->table.extraMoveProperty) {
			*countOffset += extraSize;
		}
		countOffset += 2;
	}

	return newStructId;
}

int32_t EditorT7::CreateNewRequirements()
{
	const uint16_t newStructId = m_infos->table.requirementCount;

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t newStructOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock + (uint64_t)m_infos->table.requirement + newStructId * sizeof(Requirement);

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = m_movesetSize + sizeof(Requirement) * 2;
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return -1;
	}

	// Copy all the data up to the new structure 
	memcpy(newMoveset, m_moveset, newStructOffset);

	// Initialize our structure value
	Requirement req1{ 0 };
	Requirement req2{ 0 };

	req2.condition = constants[EditorConstants_RequirementEnd];

	// Write our new structure
	memcpy(newMoveset + newStructOffset, &req1, sizeof(Requirement));
	memcpy(newMoveset + newStructOffset + sizeof(Requirement), &req2, sizeof(Requirement));

	// Copy all the data after new the new structure
	uint64_t newStructPostOffset = newStructOffset + sizeof(Requirement) * 2;
	memcpy(newMoveset + newStructPostOffset, m_moveset + newStructOffset, m_movesetSize - newStructOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	const uint64_t extraSize = sizeof(Requirement) * 2;
	m_header->offsets.animationBlock += extraSize;
	m_header->offsets.motaBlock += extraSize;
	m_infos->table.requirementCount += 2;

	// Increment moveset block offsets
	uint64_t* countOffset = (uint64_t*)&m_infos->table;
	for (size_t i = 0; i < sizeof(MovesetTable) / 8 / 2; ++i)
	{
		if (*countOffset > (uint64_t)m_infos->table.requirement) {
			*countOffset += extraSize;
		}
		countOffset += 2;
	}

	return newStructId;
}

int32_t EditorT7::CreateNewCancelList()
{
	const uint16_t newStructId = m_infos->table.cancelCount;

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t newStructOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock + (uint64_t)m_infos->table.cancel + newStructId * sizeof(Cancel);

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = m_movesetSize + sizeof(Cancel) * 2;
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return -1;
	}

	// Copy all the data up to the new structure 
	memcpy(newMoveset, m_moveset, newStructOffset);

	// Initialize our structure value
	gAddr::Cancel cancel1{ 0 };
	gAddr::Cancel cancel2{ 0 };

	cancel2.command = constants[EditorConstants_CancelCommandEnd];

	// Write our new structure
	memcpy(newMoveset + newStructOffset, &cancel1, sizeof(Cancel));
	memcpy(newMoveset + newStructOffset + sizeof(Cancel), &cancel2, sizeof(Cancel));

	// Copy all the data after new the new structure
	uint64_t newStructPostOffset = newStructOffset + sizeof(Cancel) * 2;
	memcpy(newMoveset + newStructPostOffset, m_moveset + newStructOffset, m_movesetSize - newStructOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	const uint64_t extraSize = sizeof(Cancel) * 2;
	m_header->offsets.animationBlock += extraSize;
	m_header->offsets.motaBlock += extraSize;
	m_infos->table.cancelCount += 2;

	// Increment moveset block offsets
	uint64_t* countOffset = (uint64_t*)&m_infos->table;
	for (size_t i = 0; i < sizeof(MovesetTable) / 8 / 2; ++i)
	{
		if (*countOffset > (uint64_t)m_infos->table.cancel) {
			*countOffset += extraSize;
		}
		countOffset += 2;
	}

	return newStructId;
}

int32_t EditorT7::CreateNewMove()
{
	const char* moveName = MOVESET_CUSTOM_MOVE_NAME_PREFIX;
	const size_t moveNameSize = strlen(moveName) + 1;

	const uint16_t moveId = m_infos->table.moveCount;

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	// Find position where to insert new name
	uint64_t moveNameOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock;
	const uint64_t orig_moveNameEndOffset = moveNameOffset;
	while (*(m_moveset + (moveNameOffset - 2)) == 0)
	{
		// Have to find the insert offset backward because the name block is always aligned to 8 bytes
		// We want to erase as many empty bytes because of past alignment and then re-align to 8 bytes
		moveNameOffset--;
	}

	const uint64_t relativeMoveNameOffset = moveNameOffset - m_header->offsets.nameBlock - sizeof(TKMovesetHeader);
	const uint64_t moveNameEndOffset = Helpers::align8Bytes(moveNameOffset + moveNameSize);
	const uint64_t newMoveOffset = moveNameEndOffset + (uint64_t)m_infos->table.voiceclip;
	const uint64_t origMovelistEndOffset = orig_moveNameEndOffset + (uint64_t)m_infos->table.move + moveId * sizeof(Move);

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = newMoveOffset + sizeof(Move) + (m_movesetSize - origMovelistEndOffset);
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return -1;
	}

	// Copy start //
	memcpy(newMoveset, m_moveset, moveNameOffset);

	// Write our new name
	memcpy(newMoveset + moveNameOffset, moveName, moveNameSize);

	// Copy all the data up to the new structure
	memcpy(newMoveset + moveNameEndOffset, m_moveset + orig_moveNameEndOffset, (uint64_t)m_infos->table.move + moveId * sizeof(Move));
	
	/// Move ///

	// Initialize our structure value
	uint64_t animOffset = m_animOffsetToNameOffset.begin()->first;
	uint64_t animNameOffset = m_animOffsetToNameOffset.begin()->second;

	gAddr::Move move{ 0 };
	move.name_addr = relativeMoveNameOffset;
	move.anim_name_addr = animNameOffset;
	move.anim_addr = animOffset;
	move.cancel_addr = -1;
	move._0x28_cancel_addr = -1;
	move._0x38_cancel_addr = -1;
	move._0x48_cancel_addr = -1;
	move.hit_condition_addr = -1;
	move.extra_move_property_addr = -1;
	move.move_start_extraprop_addr = -1;
	move.move_end_extraprop_addr = -1;
	move.voicelip_addr = -1;

	// Write our new structure
	memcpy(newMoveset + newMoveOffset, &move, sizeof(Move));

	// Copy all the data after new the new structure
	memcpy(newMoveset + newMoveOffset + sizeof(Move), m_moveset + origMovelistEndOffset, m_movesetSize - origMovelistEndOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	const uint64_t extraNameSize = moveNameEndOffset - orig_moveNameEndOffset;
	const uint64_t extraMoveSize = sizeof(Move);
	m_header->offsets.movesetBlock += extraNameSize;
	m_header->offsets.animationBlock += extraNameSize + extraMoveSize;
	m_header->offsets.motaBlock += extraNameSize + extraMoveSize;
	m_infos->table.moveCount++;

	// Increment moveset block offsets
	uint64_t* countOffset = (uint64_t*)&m_infos->table;
	for (size_t i = 0; i < sizeof(MovesetTable) / 8 / 2; ++i)
	{
		if (*countOffset > (uint64_t)m_infos->table.move) {
			*countOffset += extraMoveSize;
		}
		countOffset += 2;
	}

	return moveId;
}

int32_t EditorT7::CreateNew(EditorWindowType_ type)
{
	switch (type)
	{
	case EditorWindowType_Requirement:
		return CreateNewRequirements();
		break;

	case EditorWindowType_Move:
		return CreateNewMove();
		break;
	case EditorWindowType_Voiceclip:
		break;
	case EditorWindowType_Cancel:
		return CreateNewCancelList();
		break;
	case EditorWindowType_GroupedCancel:
		break;
	case EditorWindowType_CancelExtradata:
		break;

	case EditorWindowType_Extraproperty:
		return CreateNewExtraProperties();
		break;
	case EditorWindowType_MoveBeginProperty:
		break;
	case EditorWindowType_MoveEndProperty:
		break;

	case EditorWindowType_HitCondition:
		break;
	case EditorWindowType_Reactions:
		break;
	case EditorWindowType_Pushback:
		break;
	case EditorWindowType_PushbackExtradata:
		break;
	}
	return -1;
}