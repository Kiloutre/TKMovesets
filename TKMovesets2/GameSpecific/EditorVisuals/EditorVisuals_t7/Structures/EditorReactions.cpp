#include "EditorReactions.hpp"
#include "EditorVisuals.hpp"
#include "EditorVisuals_t7.hpp"

void EditorReactions::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	auto baseWindow = BaseWindow<EditorVisuals_T7>();

	int id = atoi(field->buffer);
	auto& name = field->name;

	if (name.endsWith("_pushback")) {
		m_baseWindow->OpenFormWindow(TEditorWindowType_Pushback, id);
	}
	else if (name.endsWith("_moveid")) {
		id = baseWindow->ValidateMoveId(field->buffer);
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(TEditorWindowType_Move, id);
		}
	}
}