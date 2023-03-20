#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorInputSequence.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorInputSequence::EditorInputSequence(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_InputSequence;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

void EditorInputSequence::OnFieldLabelClick(EditorInput* field)
{
	int id = atoi(field->buffer);
	std::string& name = field->name;

	if (name == "input_addr") {
		auto& inputAmountField = m_fieldIdentifierMap["input_amount"];
		if (!inputAmountField->errored) {
			m_baseWindow->OpenFormWindow(EditorWindowType_Input, id, atoi(inputAmountField->buffer));
		}
	}
}