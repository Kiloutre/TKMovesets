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