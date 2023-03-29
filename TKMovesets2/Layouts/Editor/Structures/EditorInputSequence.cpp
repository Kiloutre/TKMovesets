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

void EditorInputSequence::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	int id = atoi(field->buffer);
	std::string& name = field->name;

	if (name == "input_addr")
	{
		auto& inputAmountField = m_fieldIdentifierMap["input_amount"];
		if (!inputAmountField->errored) {
			m_baseWindow->OpenFormWindow(EditorWindowType_Input, id, atoi(inputAmountField->buffer));
		}
	}
}

void EditorInputSequence::RequestFieldUpdate(EditorWindowType_ winType, int valueChange, int listStart, int listEnd)
{
	if (winType == EditorWindowType_Input) {
		if (m_fieldIdentifierMap["input_addr"]->errored) {
			return;
		}

		int value = atoi(m_fieldIdentifierMap["input_addr"]->buffer);
		if (MUST_SHIFT_ID(value, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
            auto& field = m_fieldIdentifierMap["input_addr"];
			sprintf_s(field->buffer, field->bufsize, "%d", value + valueChange);
		}
		else if (value >= listStart && value <= (listEnd)) {
            auto& field = m_fieldIdentifierMap["input_addr"];
			value = atoi(field->buffer);
			sprintf_s(field->buffer, field->bufsize, "%d", value + valueChange);
		}
	}
}