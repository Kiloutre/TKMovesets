#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorMovelistDisplayable.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorMovelistDisplayable::EditorMovelistDisplayable(std::string windowTitleBase, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_MovelistDisplayable;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, 0, editor);
}

// -- Private methods-- //

void EditorMovelistDisplayable::OnUpdate(int listIdx, EditorInput* field)
{
	BuildItemDetails(listIdx);
	m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
}

void EditorMovelistDisplayable::BuildItemDetails(int listIdx)
{
	auto& identifierMap = m_items[listIdx]->identifierMaps;

}
