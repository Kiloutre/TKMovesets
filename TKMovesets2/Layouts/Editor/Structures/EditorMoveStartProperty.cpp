#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorMoveStartProperty.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorMoveStartProperty::EditorMoveStartProperty(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_MoveBeginProperty;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

void EditorMoveStartProperty::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	int id = atoi(field->buffer);
	std::string& name = field->name;

	if (name == "requirements_addr") {
		m_baseWindow->OpenFormWindow(EditorWindowType_Requirement, id);
	}
}

void EditorMoveStartProperty::OnApplyResize(int sizeChange, int oldSize)
{
	m_baseWindow->IssueFieldUpdate("ending_extra_properties_addr", sizeChange, id, id + oldSize);
}

void EditorMoveStartProperty::RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd)
{
	if (fieldName == "ending_extra_properties_addr") {
		// If a struct was created before this one, we must shfit our own ID
		if (MUST_SHIFT_ID(id, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
			id += valueChange;
			ApplyWindowName();
		}
	}
	else if (fieldName == "requirements_addr")
	{
		int listIdx = 0;
		for (auto& item : m_items)
		{
			EditorInput* field = item->identifierMaps[fieldName];

			if (field->errored) {
				continue;
			}

			int value = atoi(field->buffer);
			if (MUST_SHIFT_ID(value, valueChange, listStart, listEnd)) {
				// Same shifting logic as in ListCreations
				sprintf_s(field->buffer, field->bufsize, "%d", value + valueChange);
				BuildItemDetails(listIdx);
			}

			++listIdx;
		}
	}
}