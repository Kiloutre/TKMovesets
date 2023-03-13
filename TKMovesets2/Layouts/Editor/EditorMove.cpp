#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorMove.hpp"
#include "Localization.hpp"

// -- Private methods -- //

void EditorMove::Apply()
{
	if (!IsFormValid()) {
		return;
	}

	m_editor->SaveMove(id, m_inputMap);
	unsavedChanges = false;
}

// -- Public methods -- //

EditorMove::EditorMove(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	m_windowType = "move";
	m_identifierPrefix = "edition.move_field";

	std::vector<std::string> drawOrder;
	m_inputMap = editor->GetMoveInputs(t_id, drawOrder);
	InitForm(windowTitleBase, t_id, editor, drawOrder);
}
