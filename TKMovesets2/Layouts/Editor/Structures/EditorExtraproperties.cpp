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

void EditorExtraproperties::OnApplyResize(int sizeChange, int oldSize)
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
	uint64_t value = strtoll(map["value"]->buffer, nullptr, 10);

	const char* idLabel = m_baseWindow->labels->GetText(id);

	std::string startingFrameText = startingFrame == 32769 ? _("edition.extraproperty.instant") : std::to_string(startingFrame);
	std::string valueText;

	if (value > 15) {
		valueText = std::format("0x{:x} / {} / {}f", value, value, *(float*)&value);
	}
	else {
		valueText = std::to_string(value);
	}

	if (idLabel == nullptr) {
		label = std::format("{}: 0x{:x} =  {}", startingFrameText, id, valueText);
	} else {
		label = std::format("{}: {} = {}", startingFrameText, idLabel, valueText);
	}

	m_items[listIdx]->itemLabel = label;
}