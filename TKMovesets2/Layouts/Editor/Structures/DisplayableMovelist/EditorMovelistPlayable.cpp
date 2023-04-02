#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorMovelistPlayable.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorMovelistPlayable::EditorMovelistPlayable(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_MovelistPlayable;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

void EditorMovelistPlayable::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	int referenceId = atoi(field->buffer);
	std::string& name = field->name;

	if (name == "input_sequence_id") {
		if (referenceId != -1) {
			auto& inputCount = m_fieldIdentifierMap["input_count"];
			if (!inputCount->errored)
			{
				int listSize = EditorUtils::GetFieldValue(inputCount);
				m_baseWindow->OpenFormWindow(EditorWindowType_MovelistInput, referenceId, listSize);
			}
		}
	}
}