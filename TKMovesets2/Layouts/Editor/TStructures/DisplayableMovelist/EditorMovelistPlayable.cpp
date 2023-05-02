#include <ImGui.h>

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
	auto& name = field->name;

	if (name == "input_sequence_id")
	{
		if (referenceId != -1) {
			auto& inputCount = m_fieldIdentifierMap["input_count"];
			if (!inputCount->errored)
			{
				int listSize = (int)EditorUtils::GetFieldValue(inputCount);
				m_baseWindow->OpenFormWindow(EditorWindowType_MovelistInput, referenceId, listSize);
			}
		}
	}
}

void EditorMovelistPlayable::RequestFieldUpdate(EditorWindowType_ winType, int valueChange, int listStart, int listEnd)
{
	if (winType & EditorWindowType_MovelistInput)
	{
		auto& sequenceIdField = m_fieldIdentifierMap["input_sequence_id"];
		if (sequenceIdField->errored) {
			return;
		}

		int value = atoi(sequenceIdField->buffer);

		if (MUST_SHIFT_ID(value, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
			sprintf_s(sequenceIdField->buffer, sequenceIdField->bufsize, "%d", value + valueChange);
		}
		else if (value >= listStart && value <= (listEnd)) {
			auto& field = m_fieldIdentifierMap["input_count"];
			value = atoi(field->buffer);
			sprintf_s(field->buffer, field->bufsize, "%d", value + valueChange);
		}
	}
}

void EditorMovelistPlayable::OnUpdate(int listIdx, EditorInput* field)
{
	if (field->name == "input_sequence_id" || field->name == "input_count")
	{
		unsigned int list_end = (unsigned int)EditorUtils::GetFieldValue(m_fieldIdentifierMap["input_count"])
								+ (unsigned int)EditorUtils::GetFieldValue(m_fieldIdentifierMap["input_sequence_id"]);
		unsigned int inputCount = m_editor->GetMovelistDisplayableInputCount();
		m_fieldIdentifierMap["input_count"]->errored = list_end > inputCount;
	}
}