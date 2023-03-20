#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorMove.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorMove::EditorMove(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_Move;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

void EditorMove::OnFieldLabelClick(EditorInput* field)
{
	int id = atoi(field->buffer);
	std::string& name = field->name;

	if (name == "cancel_addr") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_Cancel, id);
		}
	}
	else if (name == "transition") {
		// Validation is only needed here for alias conversion
		m_baseWindow->OpenFormWindow(EditorWindowType_Move, m_baseWindow->ValidateMoveId(field->buffer));
	}
	else if (name == "voiceclip_addr") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_Voiceclip, id);
		}
	}
	else if (name == "hit_condition_addr") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_HitCondition, id);
		}
	}
	else if (name == "extra_properties_addr") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_Extraproperty, id);
		}
	}
	else if (name == "beginning_extra_properties_addr") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_MoveBeginProperty, id);
		}
	}
	else if (name == "ending_extra_properties_addr") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_MoveEndProperty, id);
		}
	}
}

void EditorMove::OnApply()
{
	m_baseWindow->ReloadMovelistFilter();
}

void EditorMove::RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd)
{
	if (fieldName == "hit_condition_addr" || fieldName == "extra_properties_addr" || fieldName == "cancel_addr" ||
		fieldName == "beginning_extra_properties_addr" || fieldName == "ending_extra_properties_addr") {
		if (!m_fieldIdentifierMap[fieldName]->errored) {
			int value = atoi(m_fieldIdentifierMap[fieldName]->buffer);
			if (value >= listEnd || (valueChange < 0 && value > listStart)) {
				// Same shifting logic as in ListCreations
				// Might be a good idea to macro it
				sprintf(m_fieldIdentifierMap[fieldName]->buffer, "%d", value + valueChange);
			}
		}
	}
}