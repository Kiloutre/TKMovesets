#include "imgui_extras.hpp"
#include "EditorReactions.hpp"
#include "Localization.hpp"
#include "helpers.hpp"

void EditorReactions::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	int id = atoi(field->buffer);
	auto& name = field->name;

	if (name.endsWith("_pushback")) {
		m_baseWindow->OpenFormWindow(EditorWindowType_Pushback, id);
	}
	else if (name.endsWith("_moveid")) {
		id = 0;//m_baseWindow->ValidateMoveId(field->buffer);
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_Move, id);
		}
	}
}