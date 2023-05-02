#include "imgui_extras.hpp"
#include "EditorReactions.hpp"
#include "Localization.hpp"
#include "helpers.hpp"

// -- Public methods -- //

EditorReactions::EditorReactions(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_Reactions;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

void EditorReactions::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	int id = atoi(field->buffer);
	auto& name = field->name;

	if (name.endsWith("_pushback")) {
		m_baseWindow->OpenFormWindow(EditorWindowType_Pushback, id);
	}
	else if (name.endsWith("_moveid")) {
		id = m_baseWindow->ValidateMoveId(field->buffer);
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_Move, id);
		}
	}
}