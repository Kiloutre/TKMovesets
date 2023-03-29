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

void EditorHitConditions::RequestFieldUpdate(EditorWindowType_ winType, int valueChange, int listStart, int listEnd)
{
	switch (winType)
	{
	case EditorWindowType_HitCondition:
		// If a struct was created before this one, we must shfit our own ID
		if (MUST_SHIFT_ID(structureId, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
			structureId += valueChange;
			ApplyWindowName();
		}
		break;

	case EditorWindowType_Requirement:
		{
			int listIdx = 0;
			for (auto& item : m_items)
			{
				EditorInput* field = item->identifierMaps["requirements_addr"];

				if (field->errored) {
					continue;
				}

				int value = atoi(field->buffer);
				if (MUST_SHIFT_ID(value, valueChange, listStart, listEnd)) {
					// Same shifting logic as in ListCreations
					// Might be a good idea to macro it
					sprintf_s(field->buffer, field->bufsize, "%d", value + valueChange);
					BuildItemDetails(listIdx);
				}

				++listIdx;
			}
		}
		break;
	}
}