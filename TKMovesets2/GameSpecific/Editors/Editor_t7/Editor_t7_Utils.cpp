#include "Editor_t7.hpp"

using namespace StructsT7;

bool EditorT7::IsCommandGroupedCancelReference(uint64_t command) const
{
	return command == constants.at(EditorConstants_GroupedCancelCommand);
}

bool EditorT7::IsCommandGroupedCancelReference(const char* buffer) const
{
	uint64_t command = (uint64_t)strtoll(buffer, nullptr, 16);
	return IsCommandGroupedCancelReference(command);
}

int EditorT7::GetCommandInputSequenceID(const char* buffer) const
{
	uint64_t command = (uint64_t)strtoll(buffer, nullptr, 16);
	return (command & 0xFFFFFFFF) - constants.at(EditorConstants_InputSequenceCommandStart);
}

bool EditorT7::IsCommandInputSequence(uint64_t command) const
{
	uint32_t sequenceId = (command & 0xFFFFFFFF);
	uint32_t inputSequenceStart = (uint32_t)constants.at(EditorConstants_InputSequenceCommandStart);
	uint32_t inputSequenceEnd = 0x10000; // Arbitrary number here. Todo: find actual limit
	if (inputSequenceStart <= sequenceId && sequenceId < inputSequenceEnd) {
		return true;
	}
	return false;
}

bool EditorT7::IsCommandInputSequence(const char* buffer) const
{
	uint64_t command = (uint64_t)strtoll(buffer, nullptr, 16);
	return IsCommandInputSequence(command);
}

bool EditorT7::IsPropertyThrowCameraRef(uint32_t id) const
{
	if (id == 0x843C || id == 0x843D) {
		return true;
	}
	return false;
}

	
bool EditorT7::IsPropertyThrowCameraRef(const char* buffer) const
{
	uint32_t id = (uint32_t)strtol(buffer, nullptr, 16);
	return IsPropertyThrowCameraRef(id);
}

bool EditorT7::IsPropertyProjectileRef(uint32_t id) const
{
	if (id == constants.at(EditorConstants_ProjectileProperty)) {
		return true;
	}
	return false;
}

bool EditorT7::IsPropertyProjectileRef(const char* buffer) const
{
	uint32_t id = (uint32_t)strtol(buffer, nullptr, 16);
	return IsPropertyProjectileRef(id);
}

bool EditorT7::IsVoicelipValueEnd(const char* buffer) const
{
	return (uint32_t)strtoll(buffer, nullptr, 16) == 0xFFFFFFFF;
}

unsigned int EditorT7::GetMotaAnimCount(int motaId) const
{
	// todo
	return 0;
}

unsigned int EditorT7::GetMovelistDisplayableInputCount() const
{
	return (unsigned int)m_iterators.mvl_inputs.size();
}


const char* EditorT7::GetMoveName(uint32_t moveid) const
{
	return (char*)(m_movesetData + m_offsets->nameBlock + m_iterators.moves[moveid]->name_addr);
}

unsigned int EditorT7::GetStructureCount(EditorWindowType type) const
{
	switch (type)
	{
	case TEditorWindowType_Move:
		return (unsigned int)m_infos->table.moveCount;
		break;

	case TEditorWindowType_Voiceclip:
		return (unsigned int)m_infos->table.voiceclipCount;
		break;

	case TEditorWindowType_HitCondition:
		return (unsigned int)m_infos->table.hitConditionCount;
		break;
	case TEditorWindowType_Reactions:
		return (unsigned int)m_infos->table.reactionsCount;
		break;
	case TEditorWindowType_Pushback:
		return (unsigned int)m_infos->table.pushbackCount;
		break;
	case TEditorWindowType_PushbackExtradata:
		return (unsigned int)m_infos->table.pushbackExtradataCount;
		break;

	case TEditorWindowType_Requirement:
		return (unsigned int)m_infos->table.requirementCount;
		break;

	case TEditorWindowType_Cancel:
		return (unsigned int)m_infos->table.cancelCount;
		break;
	case TEditorWindowType_GroupedCancel:
		return (unsigned int)m_infos->table.groupCancelCount;
		break;
	case TEditorWindowType_CancelExtradata:
		return (unsigned int)m_infos->table.cancelExtradataCount;
		break;

	case TEditorWindowType_InputSequence:
		return (unsigned int)m_infos->table.inputSequenceCount;
		break;
	case TEditorWindowType_Input:
		return (unsigned int)m_infos->table.inputCount;
		break;

	case TEditorWindowType_Extraproperty:
		return (unsigned int)m_infos->table.extraMovePropertyCount;
		break;
	case TEditorWindowType_MoveBeginProperty:
		return (unsigned int)m_infos->table.moveBeginningPropCount;
		break;
	case TEditorWindowType_MoveEndProperty:
		return (unsigned int)m_infos->table.moveEndingPropCount;
		break;

	case TEditorWindowType_Projectile:
		return (unsigned int)m_infos->table.projectileCount;
		break;

	case TEditorWindowType_CameraData:
		return (unsigned int)m_infos->table.cameraDataCount;
		break;
	case TEditorWindowType_ThrowCamera:
		return (unsigned int)m_infos->table.throwCamerasCount;
		break;
	}
	throw;
	return 0;
}