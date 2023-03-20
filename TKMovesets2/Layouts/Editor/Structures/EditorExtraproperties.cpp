#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorExtraproperties.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorExtraproperties::EditorExtraproperties(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_Extraproperty;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

void EditorExtraproperties::OnResize(int sizeChange, int oldSize)
{
	m_baseWindow->IssueFieldUpdate("extra_properties_addr", sizeChange, id, id + oldSize);
}

void EditorExtraproperties::RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd)
{
	if (fieldName == "extra_properties_addr") {
		// If a struct was created before this one, we must shfit our own ID
		if (MUST_SHIFT_ID(id, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
			id += valueChange;
			ApplyWindowName();
		}
	}
}

void EditorExtraproperties::OnUpdate(int listIdx, EditorInput* field)
{
	BuildItemDetails(listIdx);
}

void EditorExtraproperties::BuildItemDetails(int listIdx)
{
	std::string label;

	auto& map = m_items[listIdx]->identifierMaps;

	int startingFrame = atoi(map["starting_frame"]->buffer);
	int id = strtoll(map["id"]->buffer, nullptr, 16);
	int value = atoi(map["value"]->buffer);

	const char* idLabel = m_baseWindow->labels->GetText(id);

	std::string startingFrameText = startingFrame == 32769 ? _("edition.extraproperty.instant") : std::to_string(startingFrame);

	if (idLabel == nullptr) {
		label = std::format("{}: 0x{:x} = 0x{:x} / {}", startingFrameText, id, value, value);
	} else {
		label = std::format("{}: {} = 0x{:x} / {}", startingFrameText, idLabel, value, value);
	}

	m_items[listIdx]->itemLabel = label;
}