#include "TEditorStructures.hpp"
#include "EditorVisuals_t7.hpp"

void TEditorInputStruct::OnUpdate(int listIdx, EditorInput* field)
{
	BuildItemDetails(listIdx);
	m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
}

void TEditorInputStruct::BuildItemDetails(int listIdx)
{
	auto editor = Editor<EditorT7>();
	auto& identifierMap = m_items[listIdx]->identifierMap;

	m_items[listIdx]->itemLabel = editor->GetCommandStr(identifierMap["direction"]->buffer, identifierMap["button"]->buffer);
}
