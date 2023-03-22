#include <ImGui.h>
#include <format>
#include <string>

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

void EditorPushbackExtra::OnApplyResize(int sizeChange, int oldSize)
{
	m_baseWindow->IssueFieldUpdate("pushback_extra", sizeChange, id, id + oldSize);
}

void EditorPushbackExtra::RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd)
{
	if (fieldName == "pushback_extra") {
		// If a struct was created before this one, we must shift our own ID
		if (MUST_SHIFT_ID(id, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
			id += valueChange;
			ApplyWindowName();
		}
	}
}

void EditorPushbackExtra::OnUpdate(int listIdx, EditorInput* field)
{
	BuildItemDetails(listIdx);
}

void EditorPushbackExtra::BuildItemDetails(int listIdx)
{
	auto& valueBuffer = m_items[listIdx]->identifierMaps["horizontal_offset"]->buffer;
	m_items[listIdx]->itemLabel = std::format("{} = {}", _("edition.pushback_extradata.horizontal_offset"), atoi(valueBuffer));
}