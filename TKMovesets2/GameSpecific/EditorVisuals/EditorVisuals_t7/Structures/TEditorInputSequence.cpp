#include "TEditorStructures.hpp"
#include "EditorVisuals_t7.hpp"

void TEditorInputSequence::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	int id = atoi(field->buffer);
	auto& name = field->name;

	if (name == "input_addr")
	{
		auto& inputAmountField = m_fieldIdentifierMap["input_amount"];
		if (!inputAmountField->errored) {
			m_baseWindow->OpenFormWindow(TEditorWindowType_Input, id, atoi(inputAmountField->buffer));
		}
	}
}

void TEditorInputSequence::RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd)
{
	if (winType == TEditorWindowType_Input) {
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
            auto& field = m_fieldIdentifierMap["input_amount"];
			value = atoi(field->buffer);
			sprintf_s(field->buffer, field->bufsize, "%d", value + valueChange);
		}
	}
}