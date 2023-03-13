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
	m_windowTitle = std::format("{} {} - {}", _("edition.window_title_move"), t_moveId, windowTitleBase.c_str());
	moveId = t_moveId;
	m_editor = editor;

	std::vector<std::string> drawOrder;
	m_inputMap = editor->GetMoveInputs(t_moveId, drawOrder);

	for (std::string fieldName : drawOrder) {
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

	if (ImGui::Begin(m_windowTitle.c_str(), &popen, unsavedChanges ? ImGuiWindowFlags_UnsavedDocument : 0))
	{
		for (auto& field : m_inputs)
		{
			bool erroredBg = field->errored;
			if (erroredBg) {
				ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(186, 54, 54, 150));
			}

			ImGui::TextUnformatted(_(field->field_fullname.c_str()));
			ImGui::SameLine();
			ImGui::PushID(this);

			if (ImGui::InputText("##", field->buffer, sizeof(field->buffer), field->imguiInputFlags))
			{
				unsavedChanges = true;
				field->errored = m_editor->ValidateField(m_windowType, field->name, field) == false;
			} else if (ImGui::IsItemFocused() && ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
			{
				// Have to manually implement copy pasting
				// todo: make this actually work. writing to field->buffer somehow does nothing to the input text
				if (ImGui::IsKeyPressed(ImGuiKey_C, true)) {
					printf("copy - [%s]\n", field->buffer);
					ImGui::SetClipboardText(field->buffer);
				} else if (ImGui::IsKeyPressed(ImGuiKey_V, true)) {
					printf("paste - [%s]\n", field->buffer);
					field->buffer[0] = '\0';
					//strcpy_s(field->buffer, sizeof(field->buffer), ImGui::GetClipboardText());
					field->errored = m_editor->ValidateField(m_windowType, field->name, field) == false;
				}
			}
			ImGui::PopID();

			if (erroredBg) {
				ImGui::PopStyleColor();
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