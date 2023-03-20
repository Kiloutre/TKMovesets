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

void EditorRequirements::OnResize(int sizeChange, int oldSize)
{
	m_baseWindow->IssueFieldUpdate("requirements_addr", sizeChange, id, id + oldSize);
}

void EditorRequirements::RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd)
{
	if (fieldName == "requirements_addr") {
		// If a requirement was created before this one, we must shfit our own ID
		if (MUST_SHIFT_ID(id, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
			id += valueChange;
			ApplyWindowName();
		}
	}
}

void EditorRequirements::OnUpdate(int listIdx, EditorInput* field)
{
	BuildItemDetails(listIdx);
}

void EditorRequirements::BuildItemDetails(int listIdx)
{
	std::string label;

	auto& map = m_items[listIdx]->identifierMaps;

	int id = atoi(map["condition"]->buffer);
	int value = atoi(map["param"]->buffer);

	const char* idLabel = m_baseWindow->labels->GetText(id);

	if (idLabel == nullptr) {
		label = std::format("{} = 0x{:x} / {}", id, value, value);
	}
	else {
		label = std::format("{} = 0x{:x} / {}", idLabel, value, value);
	}

	m_items[listIdx]->itemLabel = label;
}