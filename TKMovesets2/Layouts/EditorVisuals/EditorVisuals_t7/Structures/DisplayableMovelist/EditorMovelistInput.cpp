#include <format>

#include "EditorMovelistInput.hpp"
#include "Localization.hpp"
#include "EditorVisuals_t7.hpp"

void EditorMovelistInput::OnUpdate(int listIdx, EditorInput* field)
{
	BuildItemDetails(listIdx);
	m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
}

void EditorMovelistInput::BuildItemDetails(int listIdx)
{
	auto editor = Editor<EditorT7>();

	auto& item = m_items[listIdx];
	auto& identifierMap = item->identifierMap;

	const auto& durationField = identifierMap["frame_duration"];

	std::string commandLabel = editor->GetDisplayableMovelistInputStr(identifierMap["directions"]->buffer, identifierMap["buttons"]->buffer);
	const char* frameLabel = EditorUtils::GetFieldValue(durationField) > 1 ? "edition.mvl_input.frames" : "edition.mvl_input.frame";
	item->itemLabel = std::format("{} {} : {}", durationField->buffer, _(frameLabel), commandLabel);
}
