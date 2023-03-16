#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorCancels.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorCancels::EditorCancels(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_Cancel;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

void EditorCancels::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	int id = atoi(field->buffer);
	std::string& name = field->name;

	if (name == "move_id") {
		uint64_t command = (uint64_t)strtoll(m_fieldIdentifierMaps[listIdx]["command"]->buffer, nullptr, 16);
		if (command == m_editor->constants[EditorConstants_GroupedCancelCommand]) {
			m_baseWindow->OpenFormWindow(EditorWindowType_GroupedCancel, id);
		}
		else {
			m_baseWindow->OpenFormWindow(EditorWindowType_Move, m_baseWindow->ValidateMoveId(field->buffer));
		}
	}
	else if (name == "extradata_addr") {
		m_baseWindow->OpenFormWindow(EditorWindowType_CancelExtradata, id);
	}
	else if (name == "requirements_addr") {
		m_baseWindow->OpenFormWindow(EditorWindowType_Requirement, id);
	}
}

void EditorCancels::OnUpdate(int listIdx, EditorInput* field)
{
	std::string& name = field->name;

	if (name == "command" || name == "move_id") {
		EditorInput* commandField = m_fieldIdentifierMaps[listIdx]["command"];
		EditorInput* moveIdField = m_fieldIdentifierMaps[listIdx]["move_id"];
		uint64_t command = (uint64_t)strtoll(commandField->buffer, nullptr, 16);

		if (command == m_editor->constants[EditorConstants_GroupedCancelCommand]) {
			int groupId = atoi(moveIdField->buffer);
			moveIdField->errored = groupId >= m_baseWindow->editorTable.groupCancelCount;
		}
		else {
			int moveId = m_baseWindow->ValidateMoveId(moveIdField->buffer);
			moveIdField->errored = moveId == -1;
		}
	}

	// todo fancy display
}