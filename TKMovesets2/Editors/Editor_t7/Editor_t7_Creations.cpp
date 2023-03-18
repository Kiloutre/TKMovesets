#include <map>
#include <format>

# include "Editor_t7.hpp"
# include "Helpers.hpp"

#define gAddr StructsT7_gameAddr

int32_t EditorT7::CreateNewPushbackExtra()
{
	const uint16_t newStructId = m_infos->table.pushbackExtradataCount;
	const size_t structSize = sizeof(PushbackExtradata);

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t newStructOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock + (uint64_t)m_infos->table.pushbackExtradata + newStructId * structSize;

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = m_movesetSize + structSize;
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return -1;
	}

	// Copy all the data up to the new structure 
	memcpy(newMoveset, m_moveset, newStructOffset);

	// Initialize our structure value
	PushbackExtradata newStruct{ 0 };

	// Write our new structure
	memcpy(newMoveset + newStructOffset, &newStruct, structSize);

	// Copy all the data after new the new structure
	uint64_t newStructPostOffset = newStructOffset + structSize;
	memcpy(newMoveset + newStructPostOffset, m_moveset + newStructOffset, m_movesetSize - newStructOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	m_header->offsets.animationBlock += structSize;
	m_header->offsets.motaBlock += structSize;
	m_infos->table.pushbackExtradataCount++;

	// Increment moveset block offsets
	uint64_t* countOffset = (uint64_t*)&m_infos->table;
	for (size_t i = 0; i < sizeof(MovesetTable) / 8 / 2; ++i)
	{
		if (*countOffset > (uint64_t)m_infos->table.pushbackExtradata) {
			*countOffset += structSize;
		}
		countOffset += 2;
	}

	return newStructId;
}

int32_t EditorT7::CreateNewPushback()
{
	const uint16_t newStructId = m_infos->table.pushbackCount;
	const size_t structSize = sizeof(Pushback);

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t newStructOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock + (uint64_t)m_infos->table.pushback + newStructId * structSize;

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = m_movesetSize + structSize;
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return -1;
	}

	// Copy all the data up to the new structure 
	memcpy(newMoveset, m_moveset, newStructOffset);

	// Initialize our structure value
	Pushback newStruct{ 0 };

	// Write our new structure
	memcpy(newMoveset + newStructOffset, &newStruct, structSize);

	// Copy all the data after new the new structure
	uint64_t newStructPostOffset = newStructOffset + structSize;
	memcpy(newMoveset + newStructPostOffset, m_moveset + newStructOffset, m_movesetSize - newStructOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	m_header->offsets.animationBlock += structSize;
	m_header->offsets.motaBlock += structSize;
	m_infos->table.pushbackCount++;

	// Increment moveset block offsets
	uint64_t* countOffset = (uint64_t*)&m_infos->table;
	for (size_t i = 0; i < sizeof(MovesetTable) / 8 / 2; ++i)
	{
		if (*countOffset > (uint64_t)m_infos->table.pushback) {
			*countOffset += structSize;
		}
		countOffset += 2;
	}

	return newStructId;
}

int32_t EditorT7::CreateNewReactions()
{
	const uint16_t newStructId = m_infos->table.reactionsCount;
	const size_t structSize = sizeof(Reactions);

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t newStructOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock + (uint64_t)m_infos->table.reactions + newStructId * structSize;

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = m_movesetSize + structSize;
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return -1;
	}

	// Copy all the data up to the new structure 
	memcpy(newMoveset, m_moveset, newStructOffset);

	// Initialize our structure value
	Reactions newStruct{ 0 };

	// Write our new structure
	memcpy(newMoveset + newStructOffset, &newStruct, structSize);

	// Copy all the data after new the new structure
	uint64_t newStructPostOffset = newStructOffset + structSize;
	memcpy(newMoveset + newStructPostOffset, m_moveset + newStructOffset, m_movesetSize - newStructOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	m_header->offsets.animationBlock += structSize;
	m_header->offsets.motaBlock += structSize;
	m_infos->table.reactionsCount++;

	// Increment moveset block offsets
	uint64_t* countOffset = (uint64_t*)&m_infos->table;
	for (size_t i = 0; i < sizeof(MovesetTable) / 8 / 2; ++i)
	{
		if (*countOffset > (uint64_t)m_infos->table.reactions) {
			*countOffset += structSize;
		}
		countOffset += 2;
	}

	return newStructId;
}

int32_t EditorT7::CreateNewHitConditions()
{
	const uint16_t newStructId = m_infos->table.hitConditionCount;
	const size_t structSize = sizeof(HitCondition);

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t newStructOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock + (uint64_t)m_infos->table.hitCondition + newStructId * structSize;

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = m_movesetSize + structSize * 2;
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return -1;
	}

	// Copy all the data up to the new structure 
	memcpy(newMoveset, m_moveset, newStructOffset);

	// Initialize our structure value
	gAddr::HitCondition hc{ 0 };
	gAddr::HitCondition hc2{ 0 };

	// This may potentially cause problems if the moveset's requirement 1 does not immediately end with a 881 requirement
	hc2.requirements_addr = 1;

	// Write our new structure
	memcpy(newMoveset + newStructOffset, &hc, structSize);
	memcpy(newMoveset + newStructOffset + structSize, &hc2, structSize);

	// Copy all the data after new the new structure
	uint64_t newStructPostOffset = newStructOffset + structSize * 2;
	memcpy(newMoveset + newStructPostOffset, m_moveset + newStructOffset, m_movesetSize - newStructOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	const uint64_t extraSize = structSize * 2;
	m_header->offsets.animationBlock += extraSize;
	m_header->offsets.motaBlock += extraSize;
	m_infos->table.hitConditionCount += 2;

	// Increment moveset block offsets
	uint64_t* countOffset = (uint64_t*)&m_infos->table;
	for (size_t i = 0; i < sizeof(MovesetTable) / 8 / 2; ++i)
	{
		if (*countOffset > (uint64_t)m_infos->table.hitCondition) {
			*countOffset += extraSize;
		}
		countOffset += 2;
	}

	return newStructId;
}

int32_t EditorT7::CreateNewExtraProperties()
{
	const uint16_t newStructId = m_infos->table.extraMovePropertyCount;
	const size_t structSize = sizeof(ExtraMoveProperty);

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t newStructOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock + (uint64_t)m_infos->table.extraMoveProperty + newStructId * structSize;

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = m_movesetSize + structSize * 2;
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
	memcpy(newMoveset + newStructOffset, &prop, structSize);
	memcpy(newMoveset + newStructOffset + structSize, &prop2, structSize);

	// Copy all the data after new the new structure
	uint64_t newStructPostOffset = newStructOffset + structSize * 2;
	memcpy(newMoveset + newStructPostOffset, m_moveset + newStructOffset, m_movesetSize - newStructOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	const uint64_t extraSize = structSize * 2;
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

int32_t EditorT7::CreateNewMoveBeginProperties()
{
	const uint16_t newStructId = m_infos->table.moveBeginningPropCount;
	const size_t structSize = sizeof(OtherMoveProperty);

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t newStructOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock + (uint64_t)m_infos->table.moveBeginningProp + newStructId * structSize;

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = m_movesetSize + structSize * 2;
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return -1;
	}

	// Copy all the data up to the new structure 
	memcpy(newMoveset, m_moveset, newStructOffset);

	// Initialize our structure value
	OtherMoveProperty prop{ 0 };
	OtherMoveProperty prop2{ 0 };

	prop2.extraprop = constants[EditorConstants_RequirementEnd];

	// Write our new structure
	memcpy(newMoveset + newStructOffset, &prop, structSize);
	memcpy(newMoveset + newStructOffset + structSize, &prop2, structSize);

	// Copy all the data after new the new structure
	uint64_t newStructPostOffset = newStructOffset + structSize * 2;
	memcpy(newMoveset + newStructPostOffset, m_moveset + newStructOffset, m_movesetSize - newStructOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	const uint64_t extraSize = structSize * 2;
	m_header->offsets.animationBlock += extraSize;
	m_header->offsets.motaBlock += extraSize;
	m_infos->table.moveBeginningPropCount += 2;

	// Increment moveset block offsets
	uint64_t* countOffset = (uint64_t*)&m_infos->table;
	for (size_t i = 0; i < sizeof(MovesetTable) / 8 / 2; ++i)
	{
		if (*countOffset > (uint64_t)m_infos->table.moveBeginningProp) {
			*countOffset += extraSize;
		}
		countOffset += 2;
	}

	return newStructId;
}

int32_t EditorT7::CreateNewMoveEndProperties()
{
	const uint16_t newStructId = m_infos->table.moveEndingPropCount;
	const size_t structSize = sizeof(OtherMoveProperty);

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t newStructOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock + (uint64_t)m_infos->table.moveEndingProp + newStructId * structSize;

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = m_movesetSize + structSize * 2;
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return -1;
	}

	// Copy all the data up to the new structure 
	memcpy(newMoveset, m_moveset, newStructOffset);

	// Initialize our structure value
	OtherMoveProperty prop{ 0 };
	OtherMoveProperty prop2{ 0 };

	prop2.extraprop = constants[EditorConstants_RequirementEnd];

	// Write our new structure
	memcpy(newMoveset + newStructOffset, &prop, structSize);
	memcpy(newMoveset + newStructOffset + structSize, &prop2, structSize);

	// Copy all the data after new the new structure
	uint64_t newStructPostOffset = newStructOffset + structSize * 2;
	memcpy(newMoveset + newStructPostOffset, m_moveset + newStructOffset, m_movesetSize - newStructOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	const uint64_t extraSize = structSize * 2;
	m_header->offsets.animationBlock += extraSize;
	m_header->offsets.motaBlock += extraSize;
	m_infos->table.moveEndingPropCount += 2;

	// Increment moveset block offsets
	uint64_t* countOffset = (uint64_t*)&m_infos->table;
	for (size_t i = 0; i < sizeof(MovesetTable) / 8 / 2; ++i)
	{
		if (*countOffset > (uint64_t)m_infos->table.moveEndingProp) {
			*countOffset += extraSize;
		}
		countOffset += 2;
	}

	return newStructId;
}


int32_t EditorT7::CreateNewRequirements()
{
	const uint16_t newStructId = m_infos->table.requirementCount;
	const size_t structSize = sizeof(Requirement);

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t newStructOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock + (uint64_t)m_infos->table.requirement + newStructId * structSize;

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = m_movesetSize + structSize * 2;
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
	memcpy(newMoveset + newStructOffset, &req1, structSize);
	memcpy(newMoveset + newStructOffset + structSize, &req2, structSize);

	// Copy all the data after new the new structure
	uint64_t newStructPostOffset = newStructOffset + structSize * 2;
	memcpy(newMoveset + newStructPostOffset, m_moveset + newStructOffset, m_movesetSize - newStructOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	const uint64_t extraSize = structSize * 2;
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

int32_t EditorT7::CreateNewVoiceclip()
{
	const uint16_t newStructId = m_infos->table.voiceclipCount;
	const size_t structSize = sizeof(Voiceclip);

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t newStructOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock + (uint64_t)m_infos->table.voiceclip + newStructId * structSize;

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = m_movesetSize + structSize;
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return -1;
	}

	// Copy all the data up to the new structure 
	memcpy(newMoveset, m_moveset, newStructOffset);

	// Initialize our structure value
	Voiceclip voiceclip{ 0 };

	// Write our new structure
	memcpy(newMoveset + newStructOffset, &voiceclip, structSize);

	// Copy all the data after new the new structure
	uint64_t newStructPostOffset = newStructOffset + structSize;
	memcpy(newMoveset + newStructPostOffset, m_moveset + newStructOffset, m_movesetSize - newStructOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	m_header->offsets.animationBlock += structSize;
	m_header->offsets.motaBlock += structSize;
	m_infos->table.voiceclipCount++;

	// Increment moveset block offsets
	uint64_t* countOffset = (uint64_t*)&m_infos->table;
	for (size_t i = 0; i < sizeof(MovesetTable) / 8 / 2; ++i)
	{
		if (*countOffset > (uint64_t)m_infos->table.voiceclip) {
			*countOffset += structSize;
		}
		countOffset += 2;
	}

	return newStructId;
}

int32_t EditorT7::CreateNewCancelExtra()
{
	const uint16_t newStructId = m_infos->table.cancelExtradataCount;
	const size_t structSize = sizeof(CancelExtradata);

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t newStructOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock + (uint64_t)m_infos->table.cancelExtradata + newStructId * structSize;

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = m_movesetSize + structSize;
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return -1;
	}

	// Copy all the data up to the new structure 
	memcpy(newMoveset, m_moveset, newStructOffset);

	// Initialize our structure value
	Voiceclip voiceclip{ 0 };

	// Write our new structure
	memcpy(newMoveset + newStructOffset, &voiceclip, structSize);

	// Copy all the data after new the new structure
	uint64_t newStructPostOffset = newStructOffset + structSize;
	memcpy(newMoveset + newStructPostOffset, m_moveset + newStructOffset, m_movesetSize - newStructOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	m_header->offsets.animationBlock += structSize;
	m_header->offsets.motaBlock += structSize;
	m_infos->table.cancelExtradataCount++;

	// Increment moveset block offsets
	uint64_t* countOffset = (uint64_t*)&m_infos->table;
	for (size_t i = 0; i < sizeof(MovesetTable) / 8 / 2; ++i)
	{
		if (*countOffset > (uint64_t)m_infos->table.cancelExtradata) {
			*countOffset += structSize;
		}
		countOffset += 2;
	}

	return newStructId;
}

int32_t EditorT7::CreateNewCancelList()
{
	const uint16_t newStructId = m_infos->table.cancelCount;
	const size_t structSize = sizeof(Cancel);

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t newStructOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock + (uint64_t)m_infos->table.cancel + newStructId * structSize;

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = m_movesetSize + structSize * 2;
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
	memcpy(newMoveset + newStructOffset, &cancel1, structSize);
	memcpy(newMoveset + newStructOffset + structSize, &cancel2, structSize);

	// Copy all the data after new the new structure
	uint64_t newStructPostOffset = newStructOffset + structSize * 2;
	memcpy(newMoveset + newStructPostOffset, m_moveset + newStructOffset, m_movesetSize - newStructOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	const uint64_t extraSize = structSize * 2;
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

int32_t EditorT7::CreateNewGroupedCancelList()
{
	const uint16_t newStructId = m_infos->table.groupCancelCount;
	const size_t structSize = sizeof(Cancel);

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	const uint64_t newStructOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock + (uint64_t)m_infos->table.groupCancel + newStructId * structSize;

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = m_movesetSize + structSize * 2;
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return -1;
	}

	// Copy all the data up to the new structure 
	memcpy(newMoveset, m_moveset, newStructOffset);

	// Initialize our structure value
	gAddr::Cancel cancel1{ 0 };
	gAddr::Cancel cancel2{ 0 };

	cancel2.command = constants[EditorConstants_GroupedCancelCommandEnd];

	// Write our new structure
	memcpy(newMoveset + newStructOffset, &cancel1, structSize);
	memcpy(newMoveset + newStructOffset + structSize, &cancel2, structSize);

	// Copy all the data after new the new structure
	uint64_t newStructPostOffset = newStructOffset + structSize * 2;
	memcpy(newMoveset + newStructPostOffset, m_moveset + newStructOffset, m_movesetSize - newStructOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	const uint64_t extraSize = structSize * 2;
	m_header->offsets.animationBlock += extraSize;
	m_header->offsets.motaBlock += extraSize;
	m_infos->table.groupCancelCount += 2;

	// Increment moveset block offsets
	uint64_t* countOffset = (uint64_t*)&m_infos->table;
	for (size_t i = 0; i < sizeof(MovesetTable) / 8 / 2; ++i)
	{
		if (*countOffset > (uint64_t)m_infos->table.groupCancel) {
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
	const size_t structSize = sizeof(Move);

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
	const uint64_t origMovelistEndOffset = orig_moveNameEndOffset + (uint64_t)m_infos->table.move + moveId * structSize;

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = newMoveOffset + structSize + (m_movesetSize - origMovelistEndOffset);
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return -1;
	}

	// Copy start //
	memcpy(newMoveset, m_moveset, moveNameOffset);

	// Write our new name
	memcpy(newMoveset + moveNameOffset, moveName, moveNameSize);

	// Copy all the data up to the new structure
	memcpy(newMoveset + moveNameEndOffset, m_moveset + orig_moveNameEndOffset, (uint64_t)m_infos->table.move + moveId * structSize);
	
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
	memcpy(newMoveset + newMoveOffset, &move, structSize);

	// Copy all the data after new the new structure
	memcpy(newMoveset + newMoveOffset + structSize, m_moveset + origMovelistEndOffset, m_movesetSize - origMovelistEndOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	const uint64_t extraNameSize = moveNameEndOffset - orig_moveNameEndOffset;
	const uint64_t extraMoveSize = structSize;
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
		return CreateNewVoiceclip();
		break;
	case EditorWindowType_Cancel:
		return CreateNewCancelList();
		break;
	case EditorWindowType_GroupedCancel:
		return CreateNewGroupedCancelList();
		break;
	case EditorWindowType_CancelExtradata:
		return CreateNewCancelExtra();
		break;

	case EditorWindowType_Extraproperty:
		return CreateNewExtraProperties();
		break;
	case EditorWindowType_MoveBeginProperty:
		return CreateNewMoveBeginProperties();
		break;
	case EditorWindowType_MoveEndProperty:
		return CreateNewMoveEndProperties();
		break;

	case EditorWindowType_HitCondition:
		return CreateNewHitConditions();
		break;
	case EditorWindowType_Reactions:
		return CreateNewReactions();
		break;
	case EditorWindowType_Pushback:
		return CreateNewPushback();
		break;
	case EditorWindowType_PushbackExtradata:
		return CreateNewPushbackExtra();
		break;
	}
	return -1;
}