#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorMove.hpp"
#include "Localization.hpp"

# define FORM_BUFFER_SIZE (32)

EditorMove::~EditorMove()
{
	// This used to deallocate stuff but doesn't because it isn't needed anymore
}

EditorMove::EditorMove(std::string windowTitleBase, uint16_t t_moveId, Editor* editor)
{
	m_windowTitle = std::format("{} {} {}", windowTitleBase.c_str(), _("edition.window_title_move"), t_moveId);
	moveId = t_moveId;
	m_editor = editor;

	std::vector<std::string> drawOrder;
	m_inputMap = editor->GetMoveInputs(t_moveId, drawOrder);

	for (std::string fieldName : drawOrder) {
		printf("%s, %lld\n", fieldName.c_str(), m_inputMap[fieldName]);
		m_inputs.push_back(m_inputMap[fieldName]);
	}

	for (auto& field : m_inputs) {
		if (field->category >= m_categoryAmount) {
			m_categoryAmount = field->category + 1;
		}
	}
}

void EditorMove::Render()
{
	if (setFocus) {
		ImGui::SetNextWindowFocus();
		setFocus = false;
	}

	// todo: anim name, anim offset

	if (ImGui::Begin(m_windowTitle.c_str(), &popen, unsavedChanges ? ImGuiWindowFlags_UnsavedDocument : 0))
	{
		for (uint8_t category = 0; category < m_categoryAmount; ++category)
		{
			if (category != 0 && !ImGui::TreeNode(_(std::format("edition.move_field.category_{}", category).c_str()))) {
				// Only show titles for category > 0, and if tree is not open: no need to render anything
				continue;
			}

			ImGui::PushItemWidth(150.0f);
			for (auto& field : m_inputs)
			{
				if (field->category != category) {
					continue;
				}

				bool erroredBg = field->errored;
				if (erroredBg) {
					ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(186, 54, 54, 150));
				}

				if (ImGui::InputText(_(field->field_fullname.c_str()), field->buffer, sizeof(field->buffer), field->imguiInputFlags))
				{
					unsavedChanges = true;
					field->errored = m_editor->ValidateField(m_fieldType, field->name, field) == false;
				}

				if (erroredBg) {
					ImGui::PopStyleColor();
				}
			}
			ImGui::PopItemWidth();

			if (category != 0) {
				ImGui::TreePop();
			}
		}

		if (ImGuiExtra::RenderButtonEnabled(_("edition.apply"), unsavedChanges)) {
			Apply();
		}
	}
	ImGui::End();
	
	if (!popen && unsavedChanges) {
		// Ordered to close, but changes remain
		// todo: show popup, force popen = true
	}
}

void EditorMove::Apply()
{
	for (auto& field : m_inputs) {
		if (field->errored) {
			//popup
			return;
		}
	}

	m_editor->SaveMove(moveId, m_inputMap);
	unsavedChanges = false;
}