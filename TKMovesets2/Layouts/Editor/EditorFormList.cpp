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

// -- Private methods -- //

void EditorFormList::Apply()
{
	if (!IsFormValid()) {
		return;
	}

	for (uint32_t listIndex = 0; listIndex < m_fieldIdentifierMaps.size(); ++listIndex) {
		m_editor->SaveItem(windowType, id + listIndex, m_fieldIdentifierMaps[listIndex]);
	}
	unsavedChanges = false;
	justAppliedChanges = true;
	m_requestedClosure = false;

	OnApply();
}

bool EditorFormList::IsFormValid()
{
	for (uint32_t listIndex = 0; listIndex < m_fieldIdentifierMaps.size(); ++listIndex)
	{
		for (auto& [category, fields] : m_fieldsCategoryMaps[listIndex]) {
			for (auto& field : fields) {
				if (field->errored) {
					return false;
				}
			}
		}
	}
	return true;
}


void EditorFormList::RenderInputs(int listIdx, std::vector<EditorInput*>& inputs, int category, int columnCount)
{
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
		RenderLabel(listIdx, field);

		// Render input field
		if (columnCount == 1) {
			ImGui::TableNextRow();
		}
		ImGui::TableNextColumn();
		RenderInput(listIdx, field);
	}
}

void EditorFormList::RenderLabel(int listIdx, EditorInput* field)
{
	const char* fieldLabel = _(field->field_fullname.c_str());
	if (field->flags & EditorInput_Clickable && !field->errored) {
		if (ImGui::Selectable(fieldLabel, true)) {
			OnFieldLabelClick(listIdx, field);
		}
	}
	else {
		ImGui::TextUnformatted(fieldLabel);
	}

	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
		// todo: maybe use this for a full-on description
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(fieldLabel);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void EditorFormList::RenderInput(int listIdx, EditorInput* field)
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
		field->errored = m_editor->ValidateField(windowType, field->name, field) == false;
		if (!field->errored) {
			OnUpdate(listIdx, field);
		}
	}
	else if (ImGui::IsItemFocused() && ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
	{
		// Have to manually implement copying
		if (ImGui::IsKeyPressed(ImGuiKey_C, true)) {
			ImGui::SetClipboardText(field->buffer);
		}
	}
	ImGui::PopID();
	ImGui::PopItemWidth();

	if (erroredBg) {
		ImGui::PopStyleColor();
	}
}

// -- Public methods -- //

void EditorFormList::InitForm(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	id = t_id;
	m_editor = editor;

	m_identifierPrefix = "edition." + EditorFormUtils::GetWindowTypeName(windowType);

	VectorSet<std::string> drawOrder;
	m_fieldIdentifierMaps = editor->GetFormFieldsList(windowType, t_id, drawOrder);

	// Tries to find a name to show in the window title
	// Also figure out the categories
	char name[32] = "";
	for (const std::string& fieldName : drawOrder) {
		EditorInput* field = m_fieldIdentifierMaps[0][fieldName];
		m_categories.insert(field->category);

		// Moves are the only named fields so there is no reason to write any more complex code for now
		if (fieldName == "move_name") {
			strcpy_s(name, sizeof(name), field->buffer);
		}
	}

	// Builds the <category : fields> maps
	for (uint32_t listIndex = 0; listIndex < m_fieldIdentifierMaps.size(); ++listIndex)
	{
		m_fieldsCategoryMaps.push_back(std::map<int, std::vector<EditorInput*>>());
		for (uint8_t category : m_categories)
		{
				std::vector<EditorInput*> inputs;
				for (const std::string& fieldName : drawOrder) {
					EditorInput* field = m_fieldIdentifierMaps[listIndex][fieldName];
					if (field->category == category) {
						inputs.push_back(field);
					}
				}
				m_fieldsCategoryMaps[listIndex][category] = inputs;
		}
	}

	// Builds the window title. Currently, switching translations does not update this. Todo 
	std::string windowName = _(std::format("{}.window_name", m_identifierPrefix).c_str());
	if (name[0] == '\0') {
		m_windowTitle = std::format("{} {} - {}", windowName, id, windowTitleBase.c_str());
	}
	else {
		m_windowTitle = std::format("{} {} {} - {}", windowName, id, name, windowTitleBase.c_str());
	}
}

void EditorFormList::RenderListControlButtons(int listIndex)
{
	ImVec2 cursor = ImGui::GetCursorPos();

	float max_x = ImGui::GetContentRegionAvail().x;

	if (listIndex == 0) {
		ImGui::SetCursorPosX(max_x - 25 * 4);
		// No point in having the + on every single item
		if (ImGui::Button("+", ImVec2(20, 20))) {
			// Insert at this position
		// This implies shifting of the entire moveset data so the editor must be called
		}
		ImGui::SameLine();
	}
	else {
		ImGui::SetCursorPosX(max_x - 25 * 3 + 5);
	}

	if (ImGui::Button("^", ImVec2(20, 20)))
	{
		// Move item UP
		// 
	}
	ImGui::SameLine();

	if (ImGui::Button("V", ImVec2(20, 20)))
	{
		// Move item DOWN
	}
	ImGui::SameLine();

	if (ImGui::Button("X", ImVec2(20, 20)))
	{
		// Delete this item
		// This implies shifting of the entire moveset data so the editor must be called
	}
	ImGui::SameLine();

	ImGui::SetCursorPos(cursor);
}

void EditorFormList::Render()
{
	if (setFocus) {
		ImGui::SetNextWindowFocus();
		setFocus = false;
	}

	if (nextDockId != -1) {
		ImGui::SetNextWindowDockID(nextDockId);
		nextDockId = -1;
	}

	if (ImGui::Begin(m_windowTitle.c_str(), &popen, unsavedChanges ? ImGuiWindowFlags_UnsavedDocument : 0))
	{
		if (m_requestedClosure)
		{
			ImGui::TextUnformatted(_("edition.discard_changes"));

			if (ImGui::Button(_("yes")))
			{
				unsavedChanges = false;
				popen = false;
			}

			if (ImGui::Button(_("no"))) {
				m_requestedClosure = false;
			}
		}
		else
		{
			lastDockId = ImGui::GetWindowDockID();

			// Responsive form that tries to use big widths to draw up to 4 fields (+ 4 labels) per line
			const int columnCount = EditorFormUtils::GetColumnCount();
			for (uint32_t listIndex = 0; listIndex < m_fieldIdentifierMaps.size(); ++listIndex)
			{
				RenderListControlButtons(listIndex);
				std::string treeNodeTitle = std::format("{} {} ({})", _(std::format("{}.window_name", m_identifierPrefix).c_str()), listIndex, listIndex + id);
				if (!ImGui::TreeNode(treeNodeTitle.c_str())) {
					// Tree node hidden so no need to render anything
					continue;
				}
				// Responsive form that tries to use big widths to draw up to 4 fields (+ 4 labels) per line
				for (uint8_t category : m_categories)
				{
					const int headerFlags = ImGuiTreeNodeFlags_Framed | (category & 1 ? 0 : ImGuiTreeNodeFlags_DefaultOpen);
					if (category != 0 && !ImGui::CollapsingHeader(_(std::format("{}.category_{}", m_identifierPrefix, category).c_str()), headerFlags)) {
						// Only show titles for category > 0, and if tree is not open: no need to render anything
						continue;
					}

					// Render each field name / field input in columns
					if (ImGui::BeginTable(m_windowTitle.c_str(), columnCount))
					{
						std::vector<EditorInput*>& inputs = m_fieldsCategoryMaps[listIndex][category];
						RenderInputs(listIndex, inputs, category, columnCount);
						ImGui::EndTable();
					}

				}

				ImGui::TreePop();
			}

			bool enabledBtn = unsavedChanges;
			if (enabledBtn) {
				ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(64, 161, 154, 255));
			}
			if (ImGuiExtra::RenderButtonEnabled(_("edition.apply"), enabledBtn, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
				Apply();
			}
			if (enabledBtn) {
				ImGui::PopStyleColor();
			}
		}
	}

	ImGui::End();

	if (!popen && unsavedChanges) {
		m_requestedClosure = true;
		popen = true;
	}
}