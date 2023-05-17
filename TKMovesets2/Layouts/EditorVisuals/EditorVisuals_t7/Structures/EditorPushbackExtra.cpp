#include <format>

#include "imgui_extras.hpp"
#include "EditorPushbackExtra.hpp"
#include "Localization.hpp"

void EditorPushbackExtra::OnUpdate(int listIdx, EditorInput* field)
{
	BuildItemDetails(listIdx);
}

void EditorPushbackExtra::BuildItemDetails(int listIdx)
{
	auto& valueBuffer = m_items[listIdx]->identifierMap["horizontal_offset"]->buffer;
	m_items[listIdx]->itemLabel = std::format("{} = {}", _("edition.pushback_extradata.horizontal_offset"), atoi(valueBuffer));
}