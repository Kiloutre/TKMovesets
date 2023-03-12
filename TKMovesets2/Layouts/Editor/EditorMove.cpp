#include <ImGui.h>
#include <format>

#include "imgui_extras.hpp"
#include "EditorMove.hpp"
#include "Localization.hpp"

# define FORM_BUFFER_SIZE (32)

EditorMove::~EditorMove()
{

	delete m_erroredFields;
}

EditorMove::EditorMove(std::string windowTitleBase, uint16_t t_moveId, Editor* editor)
{
	m_windowTitle = std::format("{} {} {}", windowTitleBase.c_str(), _("edition.window_title_move"), t_moveId);
	moveId = t_moveId;
	m_editor = editor;

	m_inputs = editor->GetFormInputs("move");

	for (auto in : m_inputs) {
		if (in->category >= m_categoryAmount) {
			m_categoryAmount = in->category + 1;
		}
	}

	m_erroredFields = new bool[m_inputs.size()] {false};
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
		//float winWidth = ImGui::GetWindowWidth();
		//printf("%f\n", winWidth);
		for (uint8_t category = 0; category < m_categoryAmount; ++category)
		{
			if (category != 0 && !ImGui::TreeNode(_(std::format("edition.move_field.category_{}", category).c_str()))) {
				continue;
			}

			ImGui::PushItemWidth(150.0f);
			for (size_t i = 0; i < m_inputs.size(); ++i)
			{
				EditorInput* field = m_inputs[i];

				if (field->category != category) {
					continue;
				}

				bool erroredBg = m_erroredFields[i];
				if (erroredBg) {
					ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(186, 54, 54, 150));
				}

				if (ImGui::InputText(_(field->name.c_str()), field->buffer, sizeof(field->buffer), field->imguiInputFlags))
				{
					unsavedChanges = true;
					m_erroredFields[i] = !m_editor->ValidateField(field->name, field->buffer);
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
	for (size_t i = 0; i < m_inputs.size(); ++i) {
		if (m_erroredFields[i]) {
			//popup
			return;
		}
	}

	unsavedChanges = false;
}