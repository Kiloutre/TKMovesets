# include "Editor_t7.hpp"
# include "Helpers.hpp"

#define gAddr StructsT7_gameAddr

uint32_t EditorT7::CreateNewProjectile()
{
	uint32_t newStructId = (uint32_t)m_infos->table.projectileCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.projectile;

	ModifyGenericMovelistListSize<Projectile>(newStructId, { -1 }, {}, listHead);

	return newStructId;
}

uint32_t EditorT7::CreateInputSequence()
{
	uint32_t newStructId = (uint32_t)m_infos->table.inputSequenceCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.inputSequence;

	ModifyGenericMovelistListSize<InputSequence>(newStructId, { -1 }, {}, listHead);

	return newStructId;
}

uint32_t EditorT7::CreateInputList()
{
	uint32_t newStructId = (uint32_t)m_infos->table.inputCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.input;

	ModifyGenericMovelistListSize<Input>(newStructId, { -1 }, {}, listHead);

	return newStructId;
}

uint32_t EditorT7::CreateNewPushbackExtra()
{
	uint32_t newStructId = (uint32_t)m_infos->table.pushbackExtradataCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.pushbackExtradata;

	ModifyGenericMovelistListSize<PushbackExtradata>(newStructId, { -1 }, {}, listHead);

	return newStructId;
}

uint32_t EditorT7::CreateNewPushback()
{
	uint32_t newStructId = (uint32_t)m_infos->table.pushbackCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.pushback;

	ModifyGenericMovelistListSize<Pushback>(newStructId, { -1 }, {}, listHead);

	return newStructId;
}

uint32_t EditorT7::CreateNewReactions()
{
	uint32_t newStructId = (uint32_t)m_infos->table.reactionsCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.reactions;

	ModifyGenericMovelistListSize<Reactions>(newStructId, { -1 }, {}, listHead);

	return newStructId;
}

uint32_t EditorT7::CreateNewHitConditions()
{
	uint32_t newStructId = (uint32_t)m_infos->table.hitConditionCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.hitCondition;

	ModifyGenericMovelistListSize<HitCondition>(newStructId, { -1 }, {}, listHead);
	auto* newStruct = m_iterators.hit_conditions[newStructId];
	// This may potentially cause problems if the moveset's requirement 1 does not immediately end with a 881 requirement
	newStruct->requirements_addr = 1;

	return newStructId;
}

uint32_t EditorT7::CreateNewExtraProperties()
{
	uint32_t newStructId = (uint32_t)m_infos->table.extraMovePropertyCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.extraMoveProperty;

	ModifyGenericMovelistListSize<ExtraMoveProperty>(newStructId, { -1, -1 }, {}, listHead);
	auto* newStruct = m_iterators.extra_move_properties[newStructId];
	newStruct[0].starting_frame = constants[EditorConstants_ExtraProperty_Instant];
	newStruct[1].starting_frame = constants[EditorConstants_ExtraPropertyEnd];

	return newStructId;
}

uint32_t EditorT7::CreateNewMoveBeginProperties()
{
	uint32_t newStructId = (uint32_t)m_infos->table.moveBeginningPropCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.moveBeginningProp;

	ModifyGenericMovelistListSize<OtherMoveProperty>(newStructId, { -1 }, {}, listHead);
	auto* newStruct = m_iterators.move_start_properties[newStructId];
	newStruct->extraprop = constants[EditorConstants_RequirementEnd];

	return newStructId;
}

uint32_t EditorT7::CreateNewMoveEndProperties()
{
	uint32_t newStructId = (uint32_t)m_infos->table.moveEndingPropCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.moveEndingProp;

	ModifyGenericMovelistListSize<OtherMoveProperty>(newStructId, { -1 }, {}, listHead);
	auto* newStruct = m_iterators.move_end_properties[newStructId];
	newStruct->extraprop = constants[EditorConstants_RequirementEnd];

	return newStructId;
}

uint32_t EditorT7::CreateNewRequirements()
{
	uint32_t newStructId = (uint32_t)m_infos->table.requirementCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.requirement;

	ModifyGenericMovelistListSize<Requirement>(newStructId, { -1, -1 }, {}, listHead);
	auto* newStruct = m_iterators.requirements[newStructId];
	newStruct[1].condition = constants[EditorConstants_RequirementEnd];

	return newStructId;
}

uint32_t EditorT7::CreateNewVoiceclipList()
{
	uint32_t newStructId = (uint32_t)m_infos->table.voiceclipCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.voiceclip;

	ModifyGenericMovelistListSize<Voiceclip>(newStructId, { -1 }, {}, listHead);
	auto* newStruct = m_iterators.voiceclips[newStructId];
	newStruct->id = (uint32_t)-1;

	return newStructId;
}

uint32_t EditorT7::CreateNewCancelExtra()
{
	uint32_t newStructId = (uint32_t)m_infos->table.cancelExtradataCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.cancelExtradata;

	ModifyGenericMovelistListSize<CancelExtradata>(newStructId, { -1 }, {}, listHead);

	return newStructId;
}

uint32_t EditorT7::CreateNewCancelList()
{
	uint32_t newStructId = (uint32_t)m_infos->table.cancelCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.cancel;

	ModifyGenericMovelistListSize<Cancel>(newStructId, { -1, -1 }, {}, listHead);
	auto* newStruct = m_iterators.cancels[newStructId];
	newStruct[1].command = constants[EditorConstants_CancelCommandEnd];

	return newStructId;
}

uint32_t EditorT7::CreateNewGroupedCancelList()
{
	uint32_t newStructId = (uint32_t)m_infos->table.groupCancelCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.groupCancel;

	ModifyGenericMovelistListSize<Cancel>(newStructId, { -1, -1 }, {}, listHead);
	auto* newStruct = m_iterators.grouped_cancels[newStructId];
	newStruct[1].command = constants[EditorConstants_GroupedCancelCommandEnd];

	return newStructId;
}

uint32_t EditorT7::CreateNewThrowCamera()
{
	uint32_t newStructId = (uint32_t)m_infos->table.throwCamerasCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.throwCameras;

	ModifyGenericMovelistListSize<ThrowCamera>(newStructId, { -1 }, {}, listHead);

	return newStructId;
}

uint32_t EditorT7::CreateNewCameraData()
{
	uint32_t newStructId = (uint32_t)m_infos->table.cameraDataCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.cameraData;

	ModifyGenericMovelistListSize<CameraData>(newStructId, { -1 }, {}, listHead);

	return newStructId;
}

uint32_t EditorT7::CreateNewMove()
{
	uint64_t nameOffset = CreateMoveName(MOVESET_CUSTOM_MOVE_NAME_PREFIX);

	uint32_t newStructId = (uint32_t)m_infos->table.moveCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.move;

	ModifyGenericMovelistListSize<Move>(newStructId, { -1 }, {}, listHead);
	auto* baseStruct = m_iterators.moves[0];
	auto* newStruct = m_iterators.moves[newStructId];
	memcpy(newStruct, baseStruct, sizeof(Move));
	newStruct->name_addr = nameOffset;

	return newStructId;
}

uint32_t EditorT7::CreateNew(EditorWindowType type)
{
	switch (type)
	{
	case TEditorWindowType_Requirement:
		return CreateNewRequirements();
		break;

	case TEditorWindowType_Move:
		return CreateNewMove();
		break;
	case TEditorWindowType_Voiceclip:
		return CreateNewVoiceclipList();
		break;
	case TEditorWindowType_Cancel:
		return CreateNewCancelList();
		break;
	case TEditorWindowType_GroupedCancel:
		return CreateNewGroupedCancelList();
		break;
	case TEditorWindowType_CancelExtradata:
		return CreateNewCancelExtra();
		break;

	case TEditorWindowType_Extraproperty:
		return CreateNewExtraProperties();
		break;
	case TEditorWindowType_MoveBeginProperty:
		return CreateNewMoveBeginProperties();
		break;
	case TEditorWindowType_MoveEndProperty:
		return CreateNewMoveEndProperties();
		break;

	case TEditorWindowType_HitCondition:
		return CreateNewHitConditions();
		break;
	case TEditorWindowType_Reactions:
		return CreateNewReactions();
		break;
	case TEditorWindowType_Pushback:
		return CreateNewPushback();
		break;
	case TEditorWindowType_PushbackExtradata:
		return CreateNewPushbackExtra();
		break;

	case TEditorWindowType_InputSequence:
		return CreateInputSequence();
		break;
	case TEditorWindowType_Input:
		return CreateInputList();
		break;

	case TEditorWindowType_Projectile:
		return CreateNewProjectile();
		break;

	case TEditorWindowType_ThrowCamera:
		return CreateNewThrowCamera();
		break;
	case TEditorWindowType_CameraData:
		return CreateNewCameraData();
		break;

	case TEditorWindowType_MovelistPlayable:
		return CreateNewMvlPlayable();
		break;
	case TEditorWindowType_MovelistInput:
		return CreateNewMvlInputs();
		break;
	}
	return -1;
}