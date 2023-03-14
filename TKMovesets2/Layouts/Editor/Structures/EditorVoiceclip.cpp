#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorVoiceclip.hpp"
#include "Localization.hpp"

// -- Private methods -- //

void EditorVoiceclip::Apply()
{
	if (!IsFormValid()) {
		return;
	}

	m_editor->SaveVoiceclip(id, m_fieldIdentifierMap);
	unsavedChanges = false;
}

// -- Public methods -- //

EditorVoiceclip::EditorVoiceclip(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	m_windowType = "voiceclip";
	m_identifierPrefix = "edition.voiceclip_field";

	VectorSet<std::string> drawOrder;
	m_fieldIdentifierMap = editor->GetVoiceclipInputs(t_id, drawOrder);
	InitForm(windowTitleBase, t_id, editor, drawOrder);
}
