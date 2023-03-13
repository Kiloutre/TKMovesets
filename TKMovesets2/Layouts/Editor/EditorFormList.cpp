#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorFormList.hpp"
#include "Localization.hpp"

// -- Static helpers -- //

static int GetColumnCount()
{
	float windowWidth = ImGui::GetWindowWidth();

	if (windowWidth > 1200) {
		return 8;
	}
	if (windowWidth > 460) {
		return 4;
	}
	if (windowWidth > 230) {
		return 2;
	}

	return 1;
}

bool EditorFormList::IsFormValid()
{
	/*
	for (auto& itemInputs : m_inputs)
	{
		for (auto& [category, fields] : itemInputs) {
			for (auto& field : fields) {
				if (field->errored) {
					return false;
				}
			}
		}
	}
	*/
	return true;
}

// -- Public methods -- //

void EditorFormList::InitForm(std::string windowTitleBase, uint32_t t_id, Editor* editor, std::vector<std::string>& drawOrder)
{
	/*
	id = t_id;
	m_editor = editor;

	// Tries to find a name to show in the window title
	// Also figure out the categories
	char name[32] = "";
	for (std::string fieldName : drawOrder) {
		EditorInput* field = m_inputMaps[0][fieldName];
		m_categories.insert(field->category);

		// Moves are the only named fields so there is no reason to write any more complex code for now
		if (fieldName == "move_name") {
			strcpy_s(name, sizeof(name), field->buffer);
		}
	}

	// Builds the <category : fields> map
	for (uint8_t category : m_categories)
	{
		std::vector<EditorInput*> inputs;
		for (std::string fieldName : drawOrder) {
			EditorInput* field = m_inputMaps[0][fieldName];
			if (field->category == category) {
				inputs.push_back(field);
			}
		}
		m_inputs[category] = inputs;
	}

	// Builds the window title. Currently, switching translations does not update this. Todo 
	std::string windowName = _(std::format("{}.window_name", m_identifierPrefix).c_str());
	if (name[0] == '\0') {
		m_windowTitle = std::format("{} {} - {}", windowName, id, windowTitleBase.c_str());
	}
	else {
		m_windowTitle = std::format("{} {} {} - {}", windowName, id, name, windowTitleBase.c_str());
	}
	*/
}

void EditorFormList::Render()
{
	if (setFocus) {
		ImGui::SetNextWindowFocus();
		setFocus = false;
	}

	if (ImGui::Begin(m_windowTitle.c_str(), &popen, unsavedChanges ? ImGuiWindowFlags_UnsavedDocument : 0))
	{
		/*
		// Responsive form that tries to use big widths to draw up to 4 fields (+ 4 labels) per line
		const int columnCount = GetColumnCount();


		for (uint8_t category : m_categories)
		{
			const int headerFlags = ImGuiTreeNodeFlags_Framed | (category & 1 ? 0 : ImGuiTreeNodeFlags_DefaultOpen);
			if (category != 0 && !ImGui::CollapsingHeader(_(std::format("{}.category_{}", m_identifierPrefix, category).c_str()), headerFlags)) {
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
					RenderLabel(field);

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
	*/
	}
	ImGui::End();
	
	if (!popen && unsavedChanges) {
		// Ordered to close, but changes remain
		// todo: show popup, force popen = true
	}
}