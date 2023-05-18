#include <format>

#include "imgui_extras.hpp"
#include "EditorMove.hpp"
#include "Localization.hpp"
#include "Helpers.hpp"
#include "EditorVisuals.hpp"
#include "EditorVisuals_t7.hpp"

void EditorMove::OnInitEnd()
{
	auto baseWindow = BaseWindow<EditorVisuals_T7>();

	// Build initial transition label
	{
		auto& field = m_fieldIdentifierMap["transition"];
		int moveId = baseWindow->ValidateMoveId(field->buffer);
		if (moveId != -1)
		{
			const char* moveName = baseWindow->movelist->at(moveId)->name.c_str();
			EditorFormUtils::SetFieldDisplayText(field, std::format("{} : {}", _(field->fullName.c_str()), moveName));
		}
	}

	// Only allow deletion of custom moves to avoid people breaking movesets by accident
	// Deletion of moves is hardly an important thing anyway
	/* -- Removed for now --
	if (strncmp(m_fieldIdentifierMap["move_name"]->buffer, "c_", 2) != 0) {
		m_isDeletable = false;
	}
	*/
}

void EditorMove::OnDuplication(unsigned int moveId, unsigned int listSize)
{
	auto baseWindow = BaseWindow<EditorVisuals_T7>();
	baseWindow->OnMoveCreate(moveId);
}

void EditorMove::ApplyWindowName(bool reapplyWindowProperties)
{
	std::string windowName = _(std::format("{}.window_name", m_identifierPrefix).c_str());
	const char* moveName = m_fieldIdentifierMap["move_name"]->buffer;
	m_windowTitle = std::format("{} {} {} - {}###{}{}{}", windowName, structureId, moveName, m_windowTitleBase.c_str(), windowName, structureId,m_windowTitleBase.c_str());
}

void EditorMove::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	auto baseWindow = BaseWindow<EditorVisuals_T7>();

	int id = atoi(field->buffer);
	auto& name = field->name;

	if (name.startsWith("cancel_addr")) {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(EditorWindowType_Cancel, id);
		}
	}
	else if (name == "transition") {
		// Validation is only needed here for alias conversion
		baseWindow->OpenFormWindow(EditorWindowType_Move, baseWindow->ValidateMoveId(field->buffer));
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
	auto baseWindow = BaseWindow<EditorVisuals_T7>();

	m_editor->RecomputeDisplayableMoveFlags(structureId);
	baseWindow->ReloadMovelistFilter();
	if (m_renamed) {
		// todo: re-compute displayable color of this move according ot its new properties
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
	default:
		return;
		break;
	}

	if (!m_fieldIdentifierMap[fieldName]->errored) {
		int currentId = atoi(m_fieldIdentifierMap[fieldName]->buffer);
		DEBUG_LOG("EditorMove::RequestFieldUpdate %u 22 - curr val is %d\n", winType, currentId);
		DEBUG_LOG("%d %d %d %d\n", currentId, valueChange, listStart, listEnd);
		if (MUST_SHIFT_ID(currentId, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
            auto& field = m_fieldIdentifierMap[fieldName];
			sprintf_s(field->buffer, field->bufsize, "%d", currentId + valueChange);
			DEBUG_LOG("Move: Shifting [%s] by %d\n", fieldName.c_str(), valueChange);
		}
	}
}

void EditorMove::OnUpdate(int listIdx, EditorInput* field)
{
	auto baseWindow = BaseWindow<EditorVisuals_T7>();

	if (field->name == "transition")
	{
		int moveId = baseWindow->ValidateMoveId(field->buffer);
		const char* moveName = baseWindow->movelist->at(moveId)->name.c_str();
		EditorFormUtils::SetFieldDisplayText(field, std::format("{} : {}", _(field->fullName.c_str()), moveName));
	}
	else if (field->name == "move_name") {
		m_renamed = true;
	}

	m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
}

void EditorMove::OpenAnimationList()
{
	m_animationListOpen = true;
	m_animationList = new EditorMove_Animations;
}

void EditorMove::PostRender()
{
	ImGui::PushID(structureId);

	if (m_animationListOpen) {
		ImGui::OpenPopup("AnimListPopup");

		if (m_animationList->Render())
		{
			const wchar_t* animPath = m_animationList->animationToImport;
			if (animPath != nullptr) {
				// User clicked on "Import" button on an anim
				std::string animName = m_editor->ImportAnimation(animPath, structureId);

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
		else
		{
			m_animationListOpen = false;
			delete m_animationList;
			m_animationList = nullptr;
		}
	}

	ImGui::PopID();
}

void EditorMove::RenderExtraContextMenuItems()
{
	ImVec2 selectableSize(0, ImGui::GetFrameHeightWithSpacing());

	ImGuiExtra::DisableBlockIf __(true);
	if (ImGui::Selectable(_("edition.form_popup.what_cancels_into_this_move"), false, 0, selectableSize))
	{
		//todo
	}
}