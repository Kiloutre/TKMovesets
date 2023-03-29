#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorInputStruct.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorInputStruct::EditorInputStruct(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow, int listSize)
{
	windowType = EditorWindowType_Input;
	m_baseWindow = baseWindow;
	m_listSize = listSize;
	InitForm(windowTitleBase, t_id, editor);
}

// -- Private methods-- //

void EditorInputStruct::OnUpdate(int listIdx, EditorInput* field)
{
	BuildItemDetails(listIdx);
	m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
}

void EditorInputStruct::BuildItemDetails(int listIdx)
{
	auto& identifierMap = m_items[listIdx]->identifierMaps;

	m_items[listIdx]->itemLabel = m_editor->GetCommandStr(identifierMap["direction"]->buffer, identifierMap["button"]->buffer);
}
