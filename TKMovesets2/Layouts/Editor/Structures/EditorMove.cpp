#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorMove.hpp"
#include "Localization.hpp"
#include "Helpers.hpp"

// -- Public methods -- //

EditorMove::EditorMove(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_Move;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);

	// Build initial transition label
	{
		auto& field = m_fieldIdentifierMap["transition"];
		int moveId = m_baseWindow->ValidateMoveId(field->buffer);
		if (moveId != -1)
		{
			const char* moveName = m_baseWindow->movelist[moveId]->name.c_str();
			EditorFormUtils::SetFieldDisplayText(field, std::format("{} : {}", _(field->fullName.c_str()), moveName));
		}
	}
}

void EditorMove::ApplyWindowName(bool reapplyWindowProperties)
{
	std::string windowName = _(std::format("{}.window_name", m_identifierPrefix).c_str());
	const char* moveName = m_fieldIdentifierMap["move_name"]->buffer;
	m_windowTitle = std::format("{} {} {} - {}", windowName, id, moveName, m_windowTitleBase.c_str());

	if (reapplyWindowProperties) {
		nextDockId = ImGui::GetWindowDockID();
		m_winInfo.applyNextRender = true;
	}
}

void EditorMove::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	int id = atoi(field->buffer);
	std::string& name = field->name;

	if (Helpers::startsWith(name, "cancel_addr")) {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_Cancel, id);
		}
	}
	else if (name == "transition") {
		// Validation is only needed here for alias conversion
		m_baseWindow->OpenFormWindow(EditorWindowType_Move, m_baseWindow->ValidateMoveId(field->buffer));
	}
	else if (name == "voiceclip_addr") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_Voiceclip, id);
		}
	}
	else if (name == "hit_condition_addr") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_HitCondition, id);
		}
	}
	else if (name == "extra_properties_addr") {
		printf("o");
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_Extraproperty, id);
		}
	}
	else if (name == "move_start_extraprop_addr") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_MoveBeginProperty, id);
		}
	}
	else if (name == "move_end_extraprop_addr") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_MoveEndProperty, id);
		}
	}
	else if (name == "anim_name")
	{
		OpenAnimationList();
	}
}

void EditorMove::OnApply()
{
	if (m_renamed) {
		m_baseWindow->ReloadMovelistFilter();
		ApplyWindowName();
		m_renamed = false;
	}
}

void EditorMove::RequestFieldUpdate(EditorWindowType_ winType, int valueChange, int listStart, int listEnd)
{

	if ((winType & (EditorWindowType_HitCondition | EditorWindowType_Extraproperty | EditorWindowType_Cancel
		| EditorWindowType_MoveBeginProperty | EditorWindowType_MoveEndProperty | EditorWindowType_Voiceclip)) == 0) {
		return;
}
	std::string fieldName;

	switch (winType)
	{
	case EditorWindowType_HitCondition:
		fieldName = "hit_condition_addr";
		break;
	case EditorWindowType_Extraproperty:
		fieldName = "extra_properties_addr";
		break;
	case EditorWindowType_Cancel:
		fieldName = "cancel_addr";
		break;
	case EditorWindowType_MoveBeginProperty:
		fieldName = "move_start_extraprop_addr";
		break;
	case EditorWindowType_MoveEndProperty:
		fieldName = "move_end_extraprop_addr";
		break;
	case EditorWindowType_Voiceclip:
		fieldName = "voiceclip_addr";
		break;
	}

	if (!m_fieldIdentifierMap[fieldName]->errored) {
		int value = atoi(m_fieldIdentifierMap[fieldName]->buffer);
		if (MUST_SHIFT_ID(value, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
            auto& field = m_fieldIdentifierMap[fieldName];
			sprintf_s(field->buffer, field->bufsize, "%d", value + valueChange);
		}
	}
}

void EditorMove::OnUpdate(int listIdx, EditorInput* field)
{
	if (field->name == "transition")
	{
		int moveId = m_baseWindow->ValidateMoveId(field->buffer);
		const char* moveName = m_baseWindow->movelist[moveId]->name.c_str();
		EditorFormUtils::SetFieldDisplayText(field, std::format("{} : {}", _(field->fullName.c_str()), moveName));
	}
	else if (field->name == "move_name") {
		m_renamed = true;
	}

	if (!m_editor->Live_OnFieldEdit(windowType, id + listIdx, field)) {
		m_baseWindow->RequireImport();
	}
}

void EditorMove::OpenAnimationList()
{
	m_animationListOpen = true;
	m_animationList = new EditorMove_Animations;
}

void EditorMove::PostRender()
{
	ImGui::PushID(id);

	if (m_animationListOpen) {
		ImGui::OpenPopup("AnimListPopup");

		if (m_animationList->Render()) {
			const char* animPath = m_animationList->animationToImport;
			if (animPath != nullptr) {
				// User clicked on "Import" button on an anim
				std::string animName = m_editor->ImportAnimation(animPath, id);

				// InmportAnimation() might return an empty name on error
				if (!animName.empty()) {
					// Update our own field
					auto& animNameField = m_fieldIdentifierMap["anim_name"];
					sprintf_s(animNameField->buffer, animNameField->bufsize, animName.c_str());
					// Mark changes as applied to tell the editor window to highlight the 'Save' button.
					// But don't update m_unsavedChanges because these changes do not require the use of the Apply() button
					justAppliedChanges = true;

					// Close the animation list now that it is not needed
					m_animationListOpen = false;
					delete m_animationList;
					m_animationList = nullptr;
				}

			}
		}
		else {
			m_animationListOpen = false;
			delete m_animationList;
			m_animationList = nullptr;
		}
	}

	ImGui::PopID();
}