#include "Editor_t7.hpp"

// Live edition callbacks that will only be called if live edition is enabled, the moveset is loaded in memory AND if the modified field is valid

void EditorT7::Live_OnMoveEdit(int id, EditorInput* field)
{
	std::string& name = field->name;

	if (name == "anim_name") {
		return ;
	}

	uint64_t blockStart = live_loadedMoveset + m_header->offsets.movesetBlock;
	uint64_t moveAddr = (uint64_t)m_infos->table.move + blockStart + id * sizeof(Move);


	if (name == "first_active_frame") {
		m_process->writeInt32(moveAddr + offsetof(Move, first_active_frame), atoi(field->buffer));
	}
	


}

void EditorT7::Live_OnCancelEdit(int id, EditorInput* field)
{
	std::string& name = field->name;
	auto& buffer = field->buffer;
	
	uint64_t blockStart = live_loadedMoveset + m_header->offsets.movesetBlock;
	uint64_t cancelAddr = blockStart + (uint64_t)m_infos->table.cancel + id * sizeof(Cancel);

	if (name == "command") {
		uint64_t command = (uint64_t)strtoll(field->buffer, nullptr, 16);
		m_process->writeInt64(cancelAddr + offsetof(Cancel, command), command);
	}
	else if (name == "requirements_addr") {
		int id = atoi(field->buffer);
		uint64_t reqAddr = blockStart + (uint64_t)m_infos->table.requirement + id * sizeof(Requirement);
		m_process->writeInt64(cancelAddr + offsetof(Cancel, requirements_addr), reqAddr);
	}
	else if (name == "extradata_addr") {
		int id = atoi(field->buffer);
		uint64_t extradataAddr = blockStart + (uint64_t)m_infos->table.cancelExtradata + id * sizeof(CancelExtradata);
		m_process->writeInt64(cancelAddr + offsetof(Cancel, extradata_addr), extradataAddr);
	}
	else if (name == "detection_start") {
		m_process->writeInt16(cancelAddr + offsetof(Cancel, detection_start), atoi(field->buffer));
	}
	else if (name == "detection_end") {
		m_process->writeInt16(cancelAddr + offsetof(Cancel, detection_end), atoi(field->buffer));
	}
	else if (name == "starting_frame") {
		m_process->writeInt16(cancelAddr + offsetof(Cancel, starting_frame), atoi(field->buffer));
	}
	else if (name == "move_id") {
		m_process->writeInt16(cancelAddr + offsetof(Cancel, move_id), atoi(field->buffer));
	} else if (name == "cancel_option") {
		m_process->writeInt16(cancelAddr + offsetof(Cancel, cancel_option), atoi(field->buffer));
	}

}

void EditorT7::Live_OnGroupedCancelEdit(int id, EditorInput* field)
{
	std::string& name = field->name;

}

void EditorT7::Live_OnExtrapropertyEdit(int id, EditorInput* field)
{
	std::string& name = field->name;

}

void EditorT7::Live_OnRequirementEdit(int id, EditorInput* field)
{
	std::string& name = field->name;

}


void EditorT7::Live_OnFieldEdit(EditorWindowType_ type, int id, EditorInput* field)
{
#ifdef BUILD_TYPE_DEBUG
	printf("Live_OnFieldEdit, type %d id %d, field name [%s], buffer [%s], loaded moveset %llx\n", type, id, field->name.c_str(), field->buffer, live_loadedMoveset);
#endif
	if (live_loadedMoveset == 0) {
		return ;
	}

	switch (type)
	{
	case EditorWindowType_Move:
		Live_OnMoveEdit(id, field);
		break;
	case EditorWindowType_Cancel:
		Live_OnCancelEdit(id, field);
		break;
	case EditorWindowType_GroupedCancel:
		Live_OnGroupedCancelEdit(id, field);
		break;
	case EditorWindowType_Extraproperty:
		Live_OnExtrapropertyEdit(id, field);
		break;
	case EditorWindowType_Requirement:
		Live_OnRequirementEdit(id, field);
		break;
	}

	return ;
}