#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorRequirements.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorRequirements::EditorRequirements(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	windowType = EditorWindowType_Requirement;
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