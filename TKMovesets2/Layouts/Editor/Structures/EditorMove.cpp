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

	if (name == "cancel_id") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_Cancel, id);
		}
	}
	else if (name == "transition") {
		// Validation is only needed here for alias conversion
		m_baseWindow->OpenFormWindow(EditorWindowType_Move, m_baseWindow->ValidateMoveId(field->buffer));
	}
	else if (name == "voiceclip_id") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_Voiceclip, id);
		}
	}
	else if (name == "hit_condition_id") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_HitCondition, id);
		}
	}
	else if (name == "extra_properties_id") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_Extraproperty, id);
		}
	}
	else if (name == "beginning_extra_properties_id") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_MoveBeginProperty, id);
		}
	}
	else if (name == "ending_extra_properties_id") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_MoveEndProperty, id);
		}
	}
}

void EditorMove::OnApply()
{
	m_baseWindow->ReloadMovelistFilter();
}