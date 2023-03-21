#include <ImGui.h>
#include <format>
#include <string>

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
	std::string& name = field->name;

	if (Helpers::endsWith(name, "_pushback")) {
		m_baseWindow->OpenFormWindow(EditorWindowType_Pushback, id);
	}
	else if (Helpers::endsWith(name, "_moveid")) {
		id = m_baseWindow->ValidateMoveId(field->buffer);
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_Move, id);
		}
	}
}