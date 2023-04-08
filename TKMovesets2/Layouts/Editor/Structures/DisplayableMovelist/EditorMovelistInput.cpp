#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorMovelistInput.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorMovelistInput::EditorMovelistInput(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow, int listSize)
{
	windowType = EditorWindowType_MovelistInput;
	m_baseWindow = baseWindow;
	m_listSize = listSize;
	InitForm(windowTitleBase, t_id, editor);
}

// -- Private methods-- //

void EditorMovelistInput::OnUpdate(int listIdx, EditorInput* field)
{
	BuildItemDetails(listIdx);
	m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
}

void EditorMovelistInput::BuildItemDetails(int listIdx)
{
	auto& item = m_items[listIdx];
	auto& identifierMap = item->identifierMap;

	const auto& durationField = identifierMap["frame_duration"];

	std::string commandLabel = m_editor->GetDisplayableMovelistInputStr(identifierMap["directions"]->buffer, identifierMap["buttons"]->buffer);
	const char* frameLabel = EditorUtils::GetFieldValue(durationField) > 1 ? "edition.mvl_input.frames" : "edition.mvl_input.frame";
	item->itemLabel = std::format("{} {} : {}", durationField->buffer, _(frameLabel), commandLabel);
}
