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
		m_winInfo.pos = ImGui::GetWindowPos();
		m_winInfo.size = ImGui::GetWindowSize();
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
	m_baseWindow->ReloadMovelistFilter();
	ApplyWindowName();
}

void EditorMove::RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd)
{
	if (fieldName == "hit_condition_addr" || fieldName == "extra_properties_addr" || fieldName == "cancel_addr" ||
		fieldName == "move_start_extraprop_addr" || fieldName == "move_end_extraprop_addr") {
		if (!m_fieldIdentifierMap[fieldName]->errored) {
			int value = atoi(m_fieldIdentifierMap[fieldName]->buffer);
			if (MUST_SHIFT_ID(value, valueChange, listStart, listEnd)) {
				// Same shifting logic as in ListCreations
				sprintf(m_fieldIdentifierMap[fieldName]->buffer, "%d", value + valueChange);
			}
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
}

void EditorMove::OpenAnimationList()
{
	m_animationListOpen = true;
}

void EditorMove::PostRender()
{
	ImGui::PushID(id);
	if (m_animationListOpen) {
		ImGui::OpenPopup("AnimListPopup");
	}
	if (ImGui::BeginPopupModal("AnimListPopup", &m_animationListOpen))
	{
		ImGui::TextUnformatted(std::format("id is {}", id).c_str());

		if (ImGui::Button(_("close"))) {
			ImGui::CloseCurrentPopup();
			m_animationListOpen = false;
		}
		ImGui::EndPopup();
	}

	ImGui::PopID();
}