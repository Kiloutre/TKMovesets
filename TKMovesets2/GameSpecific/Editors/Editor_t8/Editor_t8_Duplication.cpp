# include "Editor_t8.hpp"
# include "Helpers.hpp"

using namespace StructsT8;

#define gAddr StructsT8_gameAddr

// -- Structures -- //

uint32_t EditorT8::DuplicateMove(uint32_t id, size_t listSize)
{
	uint32_t newId = CreateNewMove();
	auto move = m_iterators.moves[newId];

	memcpy(move, m_iterators.moves[id], sizeof(Move));

	// Generate new name address + name prefix / suffix
	char const* namePtr = (char const*)(m_movesetData + m_offsets->nameBlock);

	std::string move_name = (namePtr + move->name_addr);
	if (!Helpers::startsWith<std::string>(move_name, MOVESET_CUSTOM_MOVE_NAME_PREFIX)) {
		move_name = MOVESET_CUSTOM_MOVE_NAME_PREFIX + move_name;
	}

	// Make sure the name is unique for clarity purposes
	std::string tmp = move_name;
	unsigned int number = 1;
	for (unsigned int i = 0; i < newId; ++i)
	{
		const char* moveName = namePtr + m_iterators.moves[i]->name_addr;

		if (strcmp(moveName, tmp.c_str()) == 0) {
			tmp = move_name + " (" + std::to_string(++number) + ")";
			i = -1;
		}
	}

	auto new_name_addr = CreateMoveName(tmp.c_str());
	m_iterators.moves[newId]->name_addr = new_name_addr;
	return newId;
}

uint32_t EditorT8::DuplicateCancelExtradata(uint32_t id, size_t listSize)
{
	uint32_t newId = CreateNewCancelExtra();
	memcpy(m_iterators.cancel_extras[newId], m_iterators.cancel_extras[id], sizeof(CancelExtradata));
	return newId;
}

uint32_t EditorT8::DuplicateReactions(uint32_t id, size_t listSize)
{
	uint32_t newId = CreateNewReactions();
	memcpy(m_iterators.reactions[newId], m_iterators.reactions[id], sizeof(Reactions));
	return newId;
}

uint32_t EditorT8::DuplicatePushback(uint32_t id, size_t listSize)
{
	uint32_t newId = CreateNewPushback();
	memcpy(m_iterators.pushbacks[newId], m_iterators.pushbacks[id], sizeof(Pushback));
	return newId;
}

uint32_t EditorT8::DuplicateInputSequence(uint32_t id, size_t listSize)
{
	uint32_t newId = CreateInputSequence();
	memcpy(m_iterators.input_sequences[newId], m_iterators.input_sequences[id], sizeof(InputSequence));
	return newId;
}

uint32_t EditorT8::DuplicateProjectile(uint32_t id, size_t listSize)
{
	uint32_t newId = CreateNewProjectile();
	memcpy(m_iterators.projectiles[newId], m_iterators.projectiles[id], sizeof(Projectile));
	return newId;
}

uint32_t EditorT8::DuplicateThrowCamera(uint32_t id, size_t listSize)
{
	uint32_t newId = CreateNewThrowCamera();
	memcpy(m_iterators.throw_datas[newId], m_iterators.throw_datas[id], sizeof(ThrowCamera));
	return newId;
}

uint32_t EditorT8::DuplicateCameraData(uint32_t id, size_t listSize)
{
	uint32_t newId = CreateNewCameraData();
	memcpy(m_iterators.camera_datas[newId], m_iterators.camera_datas[id], sizeof(CameraData));
	return newId;
}

uint32_t EditorT8::DuplicateMovelistPlayable(uint32_t id, size_t listSize)
{
	uint32_t newId = CreateNewMvlPlayable();
	memcpy(m_iterators.mvl_playables[newId], m_iterators.mvl_playables[id], sizeof(MvlPlayable));
	m_iterators.mvl_playables.set_size(m_iterators.mvl_playables.size() + 1);
	return newId;
}

// -- Structure lists -- //

uint32_t EditorT8::DuplicateRequirement(uint32_t id, size_t listSize)
{
	unsigned int listCount = (unsigned int)m_infos->table.requirementCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.requirement;

	ModifyGenericMovelistListSize<Requirement>(listCount, std::vector(listSize, -1), {}, listHead);
	auto& list = m_iterators.requirements;
	memcpy(list[listCount], list[id], listSize * sizeof(Requirement));

	return (uint32_t)listCount;
}

uint32_t EditorT8::DuplicateVoiceclip(uint32_t id, size_t listSize)
{
	unsigned int listCount = (unsigned int)m_infos->table.voiceclipCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.voiceclip;

	ModifyGenericMovelistListSize<Voiceclip>(listCount, std::vector(listSize, -1), {}, listHead);
	auto& list = m_iterators.voiceclips;
	memcpy(list[listCount], list[id], listSize * sizeof(Voiceclip));

	return (uint32_t)listCount;
}

uint32_t EditorT8::DuplicateCancel(uint32_t id, size_t listSize)
{
	unsigned int listCount = (unsigned int)m_infos->table.cancelCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.cancel;

	ModifyGenericMovelistListSize<Cancel>(listCount, std::vector(listSize, -1), {}, listHead);
	auto& list = m_iterators.cancels;
	memcpy(list[listCount], list[id], listSize * sizeof(Cancel));

	return (uint32_t)listCount;
}

uint32_t EditorT8::DuplicateGroupedCancel(uint32_t id, size_t listSize)
{
	unsigned int listCount = (unsigned int)m_infos->table.groupCancelCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.groupCancel;

	ModifyGenericMovelistListSize<Cancel>(listCount, std::vector(listSize, -1), {}, listHead);
	auto& list = m_iterators.grouped_cancels;
	memcpy(list[listCount], list[id], listSize * sizeof(Cancel));

	return (uint32_t)listCount;
}

uint32_t EditorT8::DuplicateExtraproperty(uint32_t id, size_t listSize)
{
	unsigned int listCount = (unsigned int)m_infos->table.extraMovePropertyCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.extraMoveProperty;

	ModifyGenericMovelistListSize<ExtraMoveProperty>(listCount, std::vector(listSize, -1), {}, listHead);
	auto& list = m_iterators.extra_move_properties;
	memcpy(list[listCount], list[id], listSize * sizeof(ExtraMoveProperty));

	return (uint32_t)listCount;
}

uint32_t EditorT8::DuplicateMoveBeginProperty(uint32_t id, size_t listSize)
{
	unsigned int listCount = (unsigned int)m_infos->table.moveBeginningPropCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.moveBeginningProp;

	ModifyGenericMovelistListSize<OtherMoveProperty>(listCount, std::vector(listSize, -1), {}, listHead);
	auto& list = m_iterators.move_start_properties;
	memcpy(list[listCount], list[id], listSize * sizeof(OtherMoveProperty));

	return (uint32_t)listCount;
}

uint32_t EditorT8::DuplicateMoveEndProperty(uint32_t id, size_t listSize)
{
	unsigned int listCount = (unsigned int)m_infos->table.moveEndingPropCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.moveEndingProp;

	ModifyGenericMovelistListSize<OtherMoveProperty>(listCount, std::vector(listSize, -1), {}, listHead);
	auto& list = m_iterators.move_end_properties;
	memcpy(list[listCount], list[id], listSize * sizeof(OtherMoveProperty));

	return (uint32_t)listCount;
}

uint32_t EditorT8::DuplicateHitCondition(uint32_t id, size_t listSize)
{
	unsigned int listCount = (unsigned int)m_infos->table.hitConditionCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.hitCondition;

	ModifyGenericMovelistListSize<HitCondition>(listCount, std::vector(listSize, -1), {}, listHead);
	auto& list = m_iterators.hit_conditions;
	memcpy(list[listCount], list[id], listSize * sizeof(HitCondition));

	return (uint32_t)listCount;
}

uint32_t EditorT8::DuplicatePushbackExtradata(uint32_t id, size_t listSize)
{
	unsigned int listCount = (unsigned int)m_infos->table.pushbackExtradataCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.pushbackExtradata;

	ModifyGenericMovelistListSize<PushbackExtradata>(listCount, std::vector(listSize, -1), {}, listHead);
	auto& list = m_iterators.pushback_extras;
	memcpy(list[listCount], list[id], listSize * sizeof(PushbackExtradata));

	return (uint32_t)listCount;
}

uint32_t EditorT8::DuplicateInput(uint32_t id, size_t listSize)
{
	unsigned int listCount = (unsigned int)m_infos->table.inputCount;
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movesetBlock + (uint64_t)m_infos->table.input;

	ModifyGenericMovelistListSize<Input>(listCount, std::vector(listSize, -1), {}, listHead);
	auto& list = m_iterators.inputs;
	memcpy(list[listCount], list[id], listSize * sizeof(Input));

	return (uint32_t)listCount;
}

uint32_t EditorT8::DuplicateMovelistInput(uint32_t id, size_t listSize)
{
	unsigned int listCount = (unsigned int)m_iterators.mvl_inputs.size();
	uint64_t listHead = m_header->moveset_data_start + m_offsets->movelistBlock + (uint64_t)m_mvlHead->inputs_offset;

	ModifyGenericMovelistListSize<MvlInput>(listCount, std::vector(listSize, -1), {}, listHead);
	auto& list = m_iterators.mvl_inputs;
	memcpy(list[listCount], list[id], listSize * sizeof(MvlInput));
	list.set_size(listCount + listSize);

	return (uint32_t)listCount;
}

uint32_t EditorT8::DuplicateStructure(EditorWindowType type, uint32_t id, size_t listSize)
{
	switch (type)
	{
	case TEditorWindowType_Move:
		return DuplicateMove(id, 1);
		break;
	case TEditorWindowType_CancelExtradata:
		return DuplicateCancelExtradata(id, 1);
		break;
	case TEditorWindowType_Reactions:
		return DuplicateReactions(id, 1);
		break;
	case TEditorWindowType_Pushback:
		return DuplicatePushback(id, 1);
		break;
	case TEditorWindowType_InputSequence:
		return DuplicateInputSequence(id, 1);
		break;
	case TEditorWindowType_Projectile:
		return DuplicateProjectile(id, 1);
		break;
	case TEditorWindowType_ThrowCamera:
		return DuplicateThrowCamera(id, 1);
		break;
	case TEditorWindowType_CameraData:
		return DuplicateCameraData(id, 1);
		break;
	case TEditorWindowType_MovelistPlayable:
		return DuplicateMovelistPlayable(id, 1);
		break;

	case TEditorWindowType_Requirement:
		return DuplicateRequirement(id, listSize);
		break;
	case TEditorWindowType_Voiceclip:
		return DuplicateVoiceclip(id, listSize);
		break;
	case TEditorWindowType_Cancel:
		return DuplicateCancel(id, listSize);
		break;
	case TEditorWindowType_GroupedCancel:
		return DuplicateGroupedCancel(id, listSize);
		break;
	case TEditorWindowType_Extraproperty:
		return DuplicateExtraproperty(id, listSize);
		break;
	case TEditorWindowType_MoveBeginProperty:
		return DuplicateMoveBeginProperty(id, listSize);
		break;
	case TEditorWindowType_MoveEndProperty:
		return DuplicateMoveEndProperty(id, listSize);
		break;
	case TEditorWindowType_HitCondition:
		return DuplicateHitCondition(id, listSize);
		break;
	case TEditorWindowType_PushbackExtradata:
		return DuplicatePushbackExtradata(id, listSize);
		break;
	case TEditorWindowType_Input:
		return DuplicateInput(id, listSize);
		break;
	case TEditorWindowType_MovelistInput:
		return DuplicateMovelistInput(id, listSize);
		break;
	}

	DEBUG_LOG("EditorT8::DuplicateStructure(): Unknown type %u\n", type);
	throw;
	return 0;
}