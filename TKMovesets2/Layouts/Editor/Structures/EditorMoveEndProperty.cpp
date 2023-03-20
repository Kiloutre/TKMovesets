#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorMoveEndProperty.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorMoveEndProperty::EditorMoveEndProperty(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_MoveEndProperty;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

void EditorMoveEndProperty::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	int id = atoi(field->buffer);
	std::string& name = field->name;

	if (name == "requirements_addr") {
		m_baseWindow->OpenFormWindow(EditorWindowType_Requirement, id);
	}
}

void EditorMoveEndProperty::OnResize(int sizeChange, int oldSize)
{
	m_baseWindow->IssueFieldUpdate("beginning_extra_properties_addr", sizeChange, id, id + oldSize);
}