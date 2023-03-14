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

	m_editor->SaveMove(id, m_fieldIdentifierMap);
	unsavedChanges = false;
}

// -- Public methods -- //

EditorMove::EditorMove(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	windowType = "move";

	VectorSet<std::string> drawOrder;
	m_fieldIdentifierMap = editor->GetMoveInputs(t_id, drawOrder);
	InitForm(windowTitleBase, t_id, editor, drawOrder);
}
