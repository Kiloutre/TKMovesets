#include "EditorReactions.hpp"
#include "EditorVisuals.hpp"
#include "EditorVisuals_t7.hpp"

void EditorReactions::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	int id = atoi(field->buffer);
	auto& name = field->name;
	auto baseWindow = static_cast<EditorVisuals_T7*>(m_baseWindow);

	if (name.endsWith("_pushback")) {
		m_baseWindow->OpenFormWindow(EditorWindowType_Pushback, id);
	}
	else if (name.endsWith("_moveid")) {
		id = baseWindow->ValidateMoveId(field->buffer);
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_Move, id);
		}
	}
}