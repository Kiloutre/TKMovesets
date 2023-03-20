#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorHitConditions.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorHitConditions::EditorHitConditions(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_HitCondition;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

void EditorHitConditions::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	int id = atoi(field->buffer);
	std::string& name = field->name;

	if (name == "requirements_addr") {
		m_baseWindow->OpenFormWindow(EditorWindowType_Requirement, id);
	}
	else if (name == "reactions_addr") {
		m_baseWindow->OpenFormWindow(EditorWindowType_Reactions, id);
	}
}

void EditorHitConditions::OnResize(int sizeChange, int oldSize)
{
	m_baseWindow->IssueFieldUpdate("hit_condition_addr", sizeChange, id, id + oldSize);
}

void EditorHitConditions::RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd)
{
	if (fieldName == "requirements_addr" || fieldName == "reactions_addr")
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
				// Might be a good idea to macro it
				sprintf(field->buffer, "%d", value + valueChange);
				BuildItemDetails(listIdx);
			}

			++listIdx;
		}
	}
}