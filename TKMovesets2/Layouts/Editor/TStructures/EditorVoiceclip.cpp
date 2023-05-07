#include <format>

#include "imgui_extras.hpp"
#include "EditorVoiceclip.hpp"
#include "Localization.hpp"

void EditorVoiceclip::OnUpdate(int listIdx, EditorInput* field)
{
	BuildItemDetails(listIdx);
	m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
}

void EditorVoiceclip::OnResize()
{
	m_editor->live_loadedMoveset = 0;
	for (int listIdx = 0; listIdx < m_listSize; ++listIdx) {
		BuildItemDetails(listIdx);
	}
}

void EditorVoiceclip::OnReorder()
{
	for (int listIdx = 0; listIdx < m_listSize; ++listIdx) {
		BuildItemDetails(listIdx);
        
        if (m_editor->live_loadedMoveset != 0) {
            for (auto& [key, field] : m_items[listIdx]->identifierMap) {
                m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
            }
        }
	}

}

void EditorVoiceclip::BuildItemDetails(int listIdx)
{
	const char* buffer = m_items[listIdx]->identifierMap["id"]->buffer;
	const bool isEnd = m_editor->IsVoicelipValueEnd(buffer);
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