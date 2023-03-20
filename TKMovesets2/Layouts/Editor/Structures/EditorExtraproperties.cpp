#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorExtraproperties.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorExtraproperties::EditorExtraproperties(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	windowType = EditorWindowType_Extraproperty;
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