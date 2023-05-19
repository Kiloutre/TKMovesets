#include "EditorPushback.hpp"
#include "EditorVisuals_t7.hpp"

void EditorPushback::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	auto& name = field->name;

	if (name == "extradata_addr")
	{
		int id = atoi(field->buffer);
		auto& amountField = m_fieldIdentifierMap["num_of_loops"];

		if (!amountField->errored) {
			int pushbackExtraAmount = atoi(amountField->buffer);
			m_baseWindow->OpenFormWindow(TEditorWindowType_PushbackExtradata, id, pushbackExtraAmount);
		}
	}
}

void EditorPushback::RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd)
{
	if (winType & TEditorWindowType_PushbackExtradata)
	{
		auto& extradataIdField = m_fieldIdentifierMap["extradata_addr"];
		if (extradataIdField->errored) {
			return;
		}

		int value = atoi(extradataIdField->buffer);

		if (MUST_SHIFT_ID(value, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
			sprintf_s(extradataIdField->buffer, extradataIdField->bufsize, "%d", value + valueChange);
		}
		else if (value >= listStart && value <= (listEnd)) {
            auto& field = m_fieldIdentifierMap["num_of_loops"];
			value = atoi(field->buffer);
			sprintf_s(field->buffer, field->bufsize, "%d", value + valueChange);
		}
	}
}