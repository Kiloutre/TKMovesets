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

void EditorVoiceclip::OnApplyResize(int sizeChange, int oldSize)
{
	m_baseWindow->IssueFieldUpdate("voiceclip_addr", sizeChange, id, id + oldSize);
}

void EditorVoiceclip::OnUpdate(int listIdx, EditorInput* field)
{
	BuildItemDetails(listIdx);
}

void EditorVoiceclip::OnResize()
{
	for (int listIdx = 0; listIdx < m_listSize; ++listIdx) {
		BuildItemDetails(listIdx);
	}
}

void EditorVoiceclip::OnReorder()
{
	for (int listIdx = 0; listIdx < m_listSize; ++listIdx) {
		BuildItemDetails(listIdx);
	}
}

void EditorVoiceclip::BuildItemDetails(int listIdx)
{
	const char* buffer = m_items[listIdx]->identifierMaps["id"]->buffer;
	const bool isEnd = strtoll(buffer, nullptr, 16) == 0xFFFFFFFF;
	std::string label;

	if (!isEnd) {
		switch (listIdx)
		{
		case 0:
			label = _("edition.voiceclip.limb_swinging");
			break;
		case 1:
			label = _("edition.voiceclip.hit");
			break;
		default:
			label = "???";
			break;
		}
	} else {
		label = _("edition.voiceclip.end");
	}

	label += std::format(" - {}", buffer);

	m_items[listIdx]->itemLabel = label;
}