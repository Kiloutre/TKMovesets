#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorExtraproperties.hpp"
#include "Localization.hpp"

// -- Private methods -- //

void EditorExtraproperties::Apply()
{
	if (!IsFormValid()) {
		return;
	}

	for (uint32_t listIndex = 0; listIndex < m_fieldIdentifierMaps.size(); ++listIndex) {
		m_editor->SaveExtraproperty(id + listIndex, m_fieldIdentifierMaps[listIndex]);
	}
	unsavedChanges = false;
	justAppliedChanges = true;
}

// -- Public methods -- //

EditorExtraproperties::EditorExtraproperties(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	windowType = "extraproperty";

	VectorSet<std::string> drawOrder;
	m_fieldIdentifierMaps = editor->GetExtrapropListInputs(t_id, drawOrder);
	InitForm(windowTitleBase, t_id, editor, drawOrder);
}
