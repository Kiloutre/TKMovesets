#include "Editor_t7.hpp"


bool EditorT7::Live_OnMoveEdit(int id, EditorInput* field)
{
	std::string& name = field->name;

	if (name == "anim_name") {
		return false;
	}

	uint64_t blockStart = game_loadedMoveset + m_header->offsets.movesetBlock;
	uint64_t moveAddr = (uint64_t)m_infos->table.move + blockStart + id * sizeof(Move);

	if (name == "first_active_frame") {
		m_process->writeInt32(moveAddr + offsetof(Move, first_active_frame), atoi(field->buffer));
	}
	


	return true;
}

bool EditorT7::Live_OnCancelEdit(int id, EditorInput* field)
{
	std::string& name = field->name;
	auto& buffer = field->buffer;
	
	uint64_t blockStart = game_loadedMoveset + m_header->offsets.movesetBlock;
	uint64_t cancelAddr = blockStart + (uint64_t)m_infos->table.cancel + id * sizeof(Cancel);

	if (name == "move_id") {
		m_process->writeInt16(cancelAddr + offsetof(Cancel, move_id), atoi(field->buffer));
	}

	return true;
}

bool EditorT7::Live_OnGroupedCancelEdit(int id, EditorInput* field)
{
	std::string& name = field->name;

	return true;
}

bool EditorT7::Live_OnExtrapropertyEdit(int id, EditorInput* field)
{
	std::string& name = field->name;

	return true;
}

bool EditorT7::Live_OnRequirementEdit(int id, EditorInput* field)
{
	std::string& name = field->name;

	return true;
}


bool EditorT7::Live_OnFieldEdit(EditorWindowType_ type, int id, EditorInput* field)
{
#ifdef BUILD_TYPE_DEBUG
	printf("Live_OnFieldEdit, type %d id %d, field name [%s], buffer [%s], loaded moveset %llx\n", type, id, field->name.c_str(), field->buffer, game_loadedMoveset);
#endif
	if (game_loadedMoveset == 0 || !m_process->IsAttached()) {
		return true;
	}

	switch (type)
	{
	case EditorWindowType_Move:
		return Live_OnMoveEdit(id, field);
		break;
	case EditorWindowType_Cancel:
		return Live_OnCancelEdit(id, field);
		break;
	case EditorWindowType_GroupedCancel:
		return Live_OnGroupedCancelEdit(id, field);
		break;
	case EditorWindowType_Extraproperty:
		return Live_OnExtrapropertyEdit(id, field);
		break;
	case EditorWindowType_Requirement:
		return Live_OnRequirementEdit(id, field);
		break;
	}

	return true;
}