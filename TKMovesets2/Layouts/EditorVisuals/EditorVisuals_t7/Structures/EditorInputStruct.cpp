#include "imgui_extras.hpp"
#include "EditorInputStruct.hpp"
#include "Localization.hpp"
#include "EditorVisuals.hpp"

void EditorInputStruct::OnUpdate(int listIdx, EditorInput* field)
{
	BuildItemDetails(listIdx);
	m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
}

void EditorInputStruct::BuildItemDetails(int listIdx)
{
	auto& identifierMap = m_items[listIdx]->identifierMap;

	m_items[listIdx]->itemLabel = m_editor->GetCommandStr(identifierMap["direction"]->buffer, identifierMap["button"]->buffer);
}
