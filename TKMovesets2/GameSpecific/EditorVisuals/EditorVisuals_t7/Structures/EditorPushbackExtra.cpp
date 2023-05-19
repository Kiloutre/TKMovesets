#include <format>

#include "TEditorStructures.hpp"
#include "Localization.hpp"
#include "EditorVisuals.hpp"

void EditorPushbackExtra::OnUpdate(int listIdx, EditorInput* field)
{
	BuildItemDetails(listIdx);
}

void EditorPushbackExtra::BuildItemDetails(int listIdx)
{
	auto& valueBuffer = m_items[listIdx]->identifierMap["horizontal_offset"]->buffer;
	m_items[listIdx]->itemLabel = std::format("{} = {}", _("edition.pushback_extradata.horizontal_offset"), atoi(valueBuffer));
}