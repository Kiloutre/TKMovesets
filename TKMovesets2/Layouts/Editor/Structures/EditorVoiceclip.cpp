#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorVoiceclip.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorVoiceclip::EditorVoiceclip(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_Voiceclip;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

void EditorVoiceclip::OnResize(int sizeChange, int oldSize)
{
	m_baseWindow->IssueFieldUpdate("voiceclip_addr", sizeChange, id, id + oldSize);
}

void EditorVoiceclip::OnUpdate(int listIdx, EditorInput* field)
{
	BuildItemDetails(listIdx);
}

void EditorVoiceclip::BuildItemDetails(int listIdx)
{
	std::string label = std::format("{}", m_items[listIdx]->identifierMaps["id"]->buffer);

	m_items[listIdx]->itemLabel = label;
}