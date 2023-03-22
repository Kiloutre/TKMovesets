#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorRequirements.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorRequirements::EditorRequirements(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_Requirement;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

void EditorRequirements::OnUpdate(int listIdx, EditorInput* field)
{
	BuildItemDetails(listIdx);
	if (!m_editor->Live_OnFieldEdit(windowType, id + listIdx, field)) {
        m_baseWindow->RequireImport();
    }
}

void EditorRequirements::BuildItemDetails(int listIdx)
{
	std::string label;

	auto& map = m_items[listIdx]->identifierMaps;

	int id = atoi(map["condition"]->buffer);
	uint64_t value = strtoll(map["param"]->buffer, nullptr, 10);

	const char* idLabel = m_baseWindow->labels->GetText(id);
	std::string valueText;

	if (value > 15) {
		valueText = std::format("0x{:x} / {} / {}f", value, value, *(float*)&value);
	}
	else {
		valueText = std::to_string(value);
	}

	if (idLabel == nullptr) {
		label = std::format("{} = {}", id, valueText);
	}
	else {
		label = std::format("{} = {}", idLabel, valueText);
	}

	m_items[listIdx]->itemLabel = label;
}