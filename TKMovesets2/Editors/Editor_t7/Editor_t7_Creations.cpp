# include "Editor_t7.hpp"
# include "Helpers.hpp"

#define gAddr StructsT7_gameAddr

template<typename T> int32_t EditorT7::CreateNewGeneric(T* struct_1, T* struct_2, size_t tableListOffset)
{
	uint64_t tableListStart = *(uint64_t*)(((Byte*)&m_infos->table) + tableListOffset);
	uint64_t tableListCount = *(uint64_t*)(((Byte*)&m_infos->table) + tableListOffset + 8);

	const uint16_t newStructId = (uint16_t)tableListCount;
	const int amount = struct_2 == nullptr ? 1 : 2;
	const uint64_t extraSize = sizeof(T) * amount;

	const uint64_t newStructOffset = m_header->infos.header_size + m_header->offsets.movesetBlock
									+ tableListStart + (newStructId * sizeof(T));

	uint64_t newMovesetSize = m_movesetSize + sizeof(T) * amount;
	Byte* newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return -1;
	}

	// Update count & table offsets right now so that iterators built from LoadMovesetPtr() are up to date
	{
		uint64_t* tableListCountPtr = (uint64_t*)(((Byte*)&m_infos->table) + tableListOffset + 8);
		*tableListCountPtr += amount;

		uint64_t* listHeadPtr = (uint64_t*)&m_infos->table;
		for (size_t i = 0; i < sizeof(MovesetTable) / 8 / 2; ++i)
		{
			if (*listHeadPtr > tableListStart) {
				*listHeadPtr += extraSize;
			}
			listHeadPtr += 2;
		}
	}

	// Copy all the data up to the new structure 
	memcpy(newMoveset, m_moveset, newStructOffset);

	// Write our new structure
	memcpy(newMoveset + newStructOffset, struct_1, sizeof(T));
	if (struct_2 != nullptr) {
		memcpy(newMoveset + newStructOffset + sizeof(T), struct_2, sizeof(T));
	}

	// Copy all the data after new the new structure
	uint64_t newStructPostOffset = newStructOffset + extraSize;
	memcpy(newMoveset + newStructPostOffset, m_moveset + newStructOffset, m_movesetSize - newStructOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	m_header->offsets.animationBlock += extraSize;
	m_header->offsets.motaBlock += extraSize;
	m_header->offsets.movelistBlock += extraSize;

	return newStructId;
}

int32_t EditorT7::CreateNewProjectile()
{
	Projectile newStruct{ 0 };

	int32_t newStructId = CreateNewGeneric<Projectile>(&newStruct, nullptr, offsetofVar(m_infos->table, projectile));
	return newStructId;
}

int32_t EditorT7::CreateInputSequence()
{
	InputSequence newStruct{ 0 };

	int32_t newStructId = CreateNewGeneric<InputSequence>(&newStruct, nullptr, offsetofVar(m_infos->table, inputSequence));
	return newStructId;
}

int32_t EditorT7::CreateInputList()
{
	Input newStruct{ 0 };
	Input newStruct2{ 0 };

	int32_t newStructId = CreateNewGeneric<Input>(&newStruct, &newStruct2, offsetofVar(m_infos->table, input));
	return newStructId;
}

int32_t EditorT7::CreateNewPushbackExtra()
{
	PushbackExtradata newStruct{ 0 };

	int32_t newStructId = CreateNewGeneric<PushbackExtradata>(&newStruct, nullptr, offsetofVar(m_infos->table, pushbackExtradata));
	return newStructId;
}

int32_t EditorT7::CreateNewPushback()
{
	Pushback newStruct{ 0 };

	int32_t newStructId = CreateNewGeneric<Pushback>(&newStruct, nullptr, offsetofVar(m_infos->table, pushback));
	return newStructId;
}

int32_t EditorT7::CreateNewReactions()
{
	Reactions newStruct{ 0 };

	int32_t newStructId = CreateNewGeneric<Reactions>(&newStruct, nullptr, offsetofVar(m_infos->table, reactions));
	return newStructId;
}

int32_t EditorT7::CreateNewHitConditions()
{
	gAddr::HitCondition newStruct{ 0 };

	// This may potentially cause problems if the moveset's requirement 1 does not immediately end with a 881 requirement
	newStruct.requirements_addr = 1;

	int32_t newStructId = CreateNewGeneric<gAddr::HitCondition>(&newStruct, &newStruct, offsetofVar(m_infos->table, hitCondition));
	return newStructId;
}

int32_t EditorT7::CreateNewExtraProperties()
{
	ExtraMoveProperty newStruct{ 0 };
	ExtraMoveProperty newStruct2{ 0 };

	newStruct2.starting_frame = constants->at(EditorConstants_ExtraPropertyEnd);

	int32_t newStructId = CreateNewGeneric<ExtraMoveProperty>(&newStruct, &newStruct2, offsetofVar(m_infos->table, extraMoveProperty));
	return newStructId;
}

int32_t EditorT7::CreateNewMoveBeginProperties()
{
	OtherMoveProperty newStruct{ 0 };
	OtherMoveProperty newStruct2{ 0 };

	newStruct2.extraprop = constants->at(EditorConstants_RequirementEnd);

	int32_t newStructId = CreateNewGeneric<OtherMoveProperty>(&newStruct, &newStruct2, offsetofVar(m_infos->table, moveBeginningProp));
	return newStructId;
}

int32_t EditorT7::CreateNewMoveEndProperties()
{
	OtherMoveProperty newStruct{ 0 };
	OtherMoveProperty newStruct2{ 0 };

	newStruct2.extraprop = constants->at(EditorConstants_RequirementEnd);

	int32_t newStructId = CreateNewGeneric<OtherMoveProperty>(&newStruct, &newStruct2, offsetofVar(m_infos->table, moveEndingProp));
	return newStructId;
}


int32_t EditorT7::CreateNewRequirements()
{
	Requirement newStruct{ 0 };
	Requirement newStruct2{ 0 };

	newStruct2.condition = constants->at(EditorConstants_RequirementEnd);

	int32_t newStructId = CreateNewGeneric<Requirement>(&newStruct, &newStruct2, offsetofVar(m_infos->table, requirement));
	return newStructId;
}

int32_t EditorT7::CreateNewVoiceclipList()
{
	Voiceclip newStruct{ 0 };
	Voiceclip newStruct2{ 0 };

	newStruct2.id = (uint32_t)-1;

	int32_t newStructId = CreateNewGeneric<Voiceclip>(&newStruct, &newStruct2, offsetofVar(m_infos->table, voiceclip));
	return newStructId;
}

int32_t EditorT7::CreateNewCancelExtra()
{
	CancelExtradata newStruct{ 0 };

	int32_t newStructId = CreateNewGeneric<CancelExtradata>(&newStruct, nullptr, offsetofVar(m_infos->table, cancelExtradata));
	return newStructId;
}

int32_t EditorT7::CreateNewCancelList()
{
	Cancel newStruct{ 0 };
	Cancel newStruct2{ 0 };

	newStruct2.command = constants->at(EditorConstants_CancelCommandEnd);

	int32_t newStructId = CreateNewGeneric<Cancel>(&newStruct, &newStruct2, offsetofVar(m_infos->table, cancel));
	return newStructId;
}

int32_t EditorT7::CreateNewGroupedCancelList()
{
	Cancel newStruct{ 0 };
	Cancel newStruct2{ 0 };

	newStruct2.command = constants->at(EditorConstants_GroupedCancelCommandEnd);

	int32_t newStructId = CreateNewGeneric<Cancel>(&newStruct, &newStruct2, offsetofVar(m_infos->table, groupCancel));
	return newStructId;
}

int32_t EditorT7::CreateNewThrowCamera()
{
	ThrowCamera newStruct{ 0 };

	int32_t newStructId = CreateNewGeneric<ThrowCamera>(&newStruct, nullptr, offsetofVar(m_infos->table, throwCameras));
	return newStructId;
}

int32_t EditorT7::CreateNewCameraData()
{
	CameraData newStruct{ 0 };

	int32_t newStructId = CreateNewGeneric<CameraData>(&newStruct, nullptr, offsetofVar(m_infos->table, cameraData));
	return newStructId;
}

int32_t EditorT7::CreateNewMove()
{
	const char* moveName = MOVESET_CUSTOM_MOVE_NAME_PREFIX;
	const size_t moveNameSize = strlen(moveName) + 1;
	const size_t structSize = sizeof(Move);

	const uint16_t moveId = (uint16_t)m_infos->table.moveCount;

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	// Find position where to insert new name
	uint64_t moveNameOffset = m_header->infos.header_size + m_header->offsets.movesetBlock;
	const uint64_t orig_nameBlockEnd = moveNameOffset;
	while (*(m_moveset + (moveNameOffset - 2)) == 0)
	{
		// Have to find the insert offset backward because the name block is always aligned to 8 bytes
		// We want to erase as many empty bytes because of past alignment and then re-align to 8 bytes
		moveNameOffset--;
	}

	const uint64_t nameBlockEnd = Helpers::align8Bytes(moveNameOffset + moveNameSize);
	const uint64_t newMove = nameBlockEnd + (uint64_t)m_infos->table.voiceclip;
	const uint64_t orig_movelistEnd = orig_nameBlockEnd + (uint64_t)m_infos->table.move + moveId * structSize;
	const uint64_t movelistSize = (uint64_t)m_infos->table.move + moveId * structSize;

	const uint64_t relativeName = moveNameOffset - m_header->offsets.nameBlock - m_header->infos.header_size;

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = newMove + structSize + (m_movesetSize - orig_movelistEnd);
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return -1;
	}

	// Shift offsets in the moveset table & in our header
	const uint64_t extraNameSize = nameBlockEnd - orig_nameBlockEnd;
	const uint64_t extraMoveSize = structSize;
	m_header->offsets.movesetBlock += extraNameSize;
	m_header->offsets.animationBlock += extraNameSize + extraMoveSize;
	m_header->offsets.motaBlock += extraNameSize + extraMoveSize;
	m_header->offsets.movelistBlock += extraNameSize + extraMoveSize;
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

	// Copy start //
	memcpy(newMoveset, m_moveset, moveNameOffset);

	// Write our new name
	memcpy(newMoveset + moveNameOffset, moveName, moveNameSize);

	// Copy all the data up to the new structure
	memcpy(newMoveset + nameBlockEnd, m_moveset + orig_nameBlockEnd, movelistSize);
	
	/// Move ///

	// Initialize our structure value
	uint64_t animOffset = m_animOffsetToNameOffset->begin()->first;
	uint64_t animNameOffset = m_animOffsetToNameOffset->begin()->second;

	gAddr::Move move{ 0 };
	move.name_addr = relativeName;
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
	memcpy(newMoveset + newMove, &move, structSize);

	// Copy all the data after new the new structure
	memcpy(newMoveset + newMove + structSize, m_moveset + orig_movelistEnd, m_movesetSize - orig_movelistEnd);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

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
		return CreateNewVoiceclipList();
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

	case EditorWindowType_InputSequence:
		return CreateInputSequence();
		break;
	case EditorWindowType_Input:
		return CreateInputList();
		break;

	case EditorWindowType_Projectile:
		return CreateNewProjectile();
		break;

	case EditorWindowType_ThrowCamera:
		return CreateNewThrowCamera();
		break;

	case EditorWindowType_CameraData:
		return CreateNewCameraData();
		break;
	}
	return -1;
}