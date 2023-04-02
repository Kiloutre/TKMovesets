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
	uniqueType = true;
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
	auto& item = m_items[listIdx];
	auto& identifierMap = item->identifierMaps;

	item->itemLabel = std::format("{} - Type: {}", listIdx, identifierMap["type"]->buffer);
}

void EditorMovelistDisplayable::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	int referenceId = atoi(field->buffer);
	std::string& name = field->name;

	if (name == "playable_id") {
		if (referenceId != -1) {
			m_baseWindow->OpenFormWindow(EditorWindowType_MovelistPlayable, referenceId);
		}
	}
}