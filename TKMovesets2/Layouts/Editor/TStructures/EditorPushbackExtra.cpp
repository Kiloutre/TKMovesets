#include <format>

#include "imgui_extras.hpp"
#include "EditorPushbackExtra.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorPushbackExtra::EditorPushbackExtra(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow, int listSize)
{
	windowType = EditorWindowType_PushbackExtradata;
	m_baseWindow = baseWindow;
	m_listSize = listSize;
	InitForm(windowTitleBase, t_id, editor);
}

void EditorPushbackExtra::OnUpdate(int listIdx, EditorInput* field)
{
	BuildItemDetails(listIdx);
}

void EditorPushbackExtra::BuildItemDetails(int listIdx)
{
	auto& valueBuffer = m_items[listIdx]->identifierMap["horizontal_offset"]->buffer;
	m_items[listIdx]->itemLabel = std::format("{} = {}", _("edition.pushback_extradata.horizontal_offset"), atoi(valueBuffer));
}