#include "TEditorStructures.hpp"
#include "EditorVisuals_t7.hpp"

void TEditorProjectile::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	int id = atoi(field->buffer);
	auto& name = field->name;

	if (name == "cancel_addr") {
		m_baseWindow->OpenFormWindow(TEditorWindowType_Cancel, id);
	}
	else if (name == "hit_condition_addr") {
		m_baseWindow->OpenFormWindow(TEditorWindowType_HitCondition, id);
	}
}

void TEditorProjectile::RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd)
{
	
	if (winType & (TEditorWindowType_Cancel | TEditorWindowType_HitCondition))
{
		std::string fieldName = (winType & TEditorWindowType_Cancel) ? "cancel_addr" : "hit_condition_addr";
		EditorInput* field = m_fieldIdentifierMap[fieldName];

		if (field->errored) {
			return;
		}

		int value = atoi(field->buffer);
		if (MUST_SHIFT_ID(value, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
			// Might be a good idea to macro it
			sprintf_s(field->buffer, field->bufsize, "%d", value + valueChange);
		}
	}
}