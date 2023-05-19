#include "TEditorStructures.hpp"
#include "EditorVisuals_t7.hpp"

void TEditorMoveStartProperty::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	int id = atoi(field->buffer);
	auto& name = field->name;

	if (name == "requirements_addr") {
		m_baseWindow->OpenFormWindow(TEditorWindowType_Requirement, id);
	}
}


void TEditorMoveStartProperty::RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd)
{
	switch (winType)
	{
	case TEditorWindowType_MoveBeginProperty:
		// If a struct was created before this one, we must shfit our own ID
		if (MUST_SHIFT_ID(structureId, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
			structureId += valueChange;
			ApplyWindowName();
		}
		break;
	case TEditorWindowType_Requirement:
	{
		int listIdx = 0;
		for (auto& item : m_items)
		{
			EditorInput* field = item->identifierMap["requirements_addr"];

			if (field->errored) {
				continue;
			}

			int value = atoi(field->buffer);
			if (MUST_SHIFT_ID(value, valueChange, listStart, listEnd)) {
				// Same shifting logic as in ListCreations
				sprintf_s(field->buffer, field->bufsize, "%d", value + valueChange);
				BuildItemDetails(listIdx);
			}

			++listIdx;
		}
	}
	break;
	}
}