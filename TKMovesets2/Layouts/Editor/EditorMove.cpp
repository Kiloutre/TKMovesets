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
	moveId = t_moveId;
	m_editor = editor;

	std::vector<std::string> drawOrder;
	m_inputMap = editor->GetMoveInputs(t_moveId, drawOrder);

	// Tries to find a name to show in the window title
	// Also figure out the max amount of categories
	char name[32] = "";
	for (std::string fieldName : drawOrder) {
		EditorInput* field = m_inputMap[fieldName];
		if (field->category >= m_categoryAmount) {
			m_categoryAmount = field->category + 1;
		}
		if (fieldName == "move_name") {
			strcpy_s(name, sizeof(name), field->buffer);
		}
	}

	// Builds the <category : fields> map
	for (size_t i = 0; i < m_categoryAmount; ++i)
	{
		std::vector<EditorInput*> inputs;
		for (std::string fieldName : drawOrder) {
			EditorInput* field = m_inputMap[fieldName];
			if (field->category == i) {
				inputs.push_back(field);
			}
		}
		m_inputs[i] = inputs;
	}

	// Builds the window title. Currently, switching translations does not update this. Todo 
	if (name[0] == '\0') {
		m_windowTitle = std::format("{} {} - {}", _("edition.window_title_move"), t_moveId, windowTitleBase.c_str());
	}
	else {
		m_windowTitle = std::format("{} {} {} - {}", _("edition.window_title_move"), t_moveId, name, windowTitleBase.c_str());
	}
}

void EditorMove::RenderInput(EditorInput* field)
{
	bool erroredBg = field->errored;
	if (erroredBg) {
		ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(186, 54, 54, 150));
	}

	ImGui::PushID(field);
	ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
	if (ImGui::InputText("##", field->buffer, sizeof(field->buffer), field->imguiInputFlags))
	{
		unsavedChanges = true;
		field->errored = m_editor->ValidateField(m_windowType, field->name, field) == false;
	}
	else if (ImGui::IsItemFocused() && ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
	{
		// Have to manually implement copy pasting
		// todo: make this actually work. writing to field->buffer somehow does nothing to the input text
		if (ImGui::IsKeyPressed(ImGuiKey_C, true)) {
			printf("copy - [%s]\n", field->buffer);
			ImGui::SetClipboardText(field->buffer);
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_V, true)) {
			printf("paste - [%s]\n", field->buffer);
			field->buffer[0] = '\0';
			//strcpy_s(field->buffer, sizeof(field->buffer), ImGui::GetClipboardText());
			field->errored = m_editor->ValidateField(m_windowType, field->name, field) == false;
		}
	}
	ImGui::PopID();
	ImGui::PopItemWidth();

	if (erroredBg) {
		ImGui::PopStyleColor();
	}
}

static int GetColumnCount()
{
	float windowWidth = ImGui::GetWindowWidth();

	if (windowWidth > 1200) {
		return 8;
	}
	if (windowWidth > 600) {
		return 4;
	}
	if (windowWidth > 230) {
		return 2;
	}

	return 1;
}

void EditorMove::Render()
{
	if (setFocus) {
		ImGui::SetNextWindowFocus();
		setFocus = false;
	}

	if (ImGui::Begin(m_windowTitle.c_str(), &popen, unsavedChanges ? ImGuiWindowFlags_UnsavedDocument : 0))
	{
		// Responsive form that tries to use big widths to draw up to 4 fields (+ 4 labels) per line
		const int columnCount = GetColumnCount();
		for (int category = 0; category < m_categoryAmount; ++category)
		{
			if (category != 0 && !ImGui::CollapsingHeader(_(std::format("edition.move_field.category_{}", category).c_str()))) {
				// Only show titles for category > 0, and if tree is not open: no need to render anything
				continue;
			}

			if (ImGui::BeginTable(m_windowTitle.c_str(), columnCount))
			{
				std::vector<EditorInput*>& inputs = m_inputs[category];
				for (size_t i = 0; i < inputs.size(); ++i)
				{
					EditorInput* field = inputs[i];

					if (field->category != category) {
						continue;
					}
					//Render label
					if (i % columnCount == 0) {
						ImGui::TableNextRow();
					}
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(_(field->field_fullname.c_str()));

					// Render input field
					if (columnCount == 1) {
						ImGui::TableNextRow();
					}
					ImGui::TableNextColumn();
					RenderInput(field);
				}
				ImGui::EndTable();
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
	for (auto& [category, fields] : m_inputs) {
		for (auto& field : fields) {
			if (field->errored) {
				//popup
				return;
			}
		}
	}

	m_editor->SaveMove(moveId, m_inputMap);
	unsavedChanges = false;
}