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

	if (m_listSizeChange != 0)
	{
		// If items were added/removed, reallocate entire moveset
		m_editor->ModifyListSize(windowType, id, m_listSize - m_listSizeChange, m_listSize);
		m_listSizeChange = 0;
	}

	unsavedChanges = false;
	justAppliedChanges = true;
	m_requestedClosure = false;

	if (m_listSize == 0) {
		// List is now empty, no nede to kepe this open
		popen = false;
		return;
	}

	// Write into every individual item
	for (uint32_t listIndex = 0; listIndex < m_listSize; ++listIndex) {
		m_editor->SaveItem(windowType, id + listIndex, m_fieldIdentifierMaps[listIndex]);
	}

	OnApply();
}

bool EditorFormList::IsFormValid()
{
	for (uint32_t listIndex = 0; listIndex < m_listSize; ++listIndex)
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
	m_listSize = m_fieldIdentifierMaps.size();

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

	// Builds the <category : fields> maps & the item labels
	m_itemLabels = std::vector<std::string>(m_listSize);
	for (uint32_t listIndex = 0; listIndex < m_listSize; ++listIndex)
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

		BuildItemLabel(listIndex);
	}

	// Build category names
	for (uint8_t category : m_categories) {
		m_categoryStringIdentifiers[category] = std::format("{}.category_{}", m_identifierPrefix, category);
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

	const ImVec2 buttonSize(18, 18);
	const float buttonWidth = 25;
	float pos_x = ImGui::GetContentRegionAvail().x - buttonWidth * 4;

	ImGui::PushID(this);
	ImGui::PushID(listIndex);

	// No point in having the + on every single item
	if (listIndex + 1 != m_listSize || m_listSize == 1) {
		ImGui::SetCursorPosX(pos_x);
		if (ImGui::Button("+", ImVec2(20, 20))) {
			VectorSet<std::string> drawOrder;
			auto fieldMap = m_editor->GetFormFieldsList(windowType, 0, drawOrder)[0];

			m_fieldIdentifierMaps.insert(m_fieldIdentifierMaps.begin() + listIndex, fieldMap);

			for (uint8_t category : m_categories)
			{
				m_fieldsCategoryMaps.insert(m_fieldsCategoryMaps.begin() + listIndex, std::map<int, std::vector<EditorInput*>>());
				std::vector<EditorInput*> inputs;
				for (const std::string& fieldName : drawOrder) {
					EditorInput* field = fieldMap[fieldName];
					if (field->category == category) {
						inputs.push_back(field);
					}
				}
				m_fieldsCategoryMaps[listIndex][category] = inputs;
			}

			++m_listSizeChange;
			++m_listSize;

			m_itemLabels.push_back("");
			for (int i = listIndex; i < m_listSize; ++i) {
				BuildItemLabel(i);
			}
			unsavedChanges = true;
		}
		ImGui::SameLine();
	}
	
	// Don't allow first item and ending item to shift up
	if (listIndex != 0 && listIndex + 1 != m_listSize) {
		ImGui::SetCursorPosX(pos_x + buttonWidth);
		if (ImGui::Button("^", buttonSize))
		{
			// Move item UP
			std::iter_swap(m_fieldIdentifierMaps.begin() + listIndex, m_fieldIdentifierMaps.begin() + listIndex - 1);
			std::iter_swap(m_fieldsCategoryMaps.begin() + listIndex, m_fieldsCategoryMaps.begin() + listIndex - 1);
			BuildItemLabel(listIndex);
			BuildItemLabel(listIndex - 1);
			unsavedChanges = true;
		}
		ImGui::SameLine();
	}

	// Don't allow last 2 items to shift down
	if (listIndex + 2 < m_listSize) {
		ImGui::SetCursorPosX(pos_x + buttonWidth * 2);
		if (ImGui::Button("V", buttonSize))
		{
			// Move item DOWN
			std::iter_swap(m_fieldIdentifierMaps.begin() + listIndex, m_fieldIdentifierMaps.begin() + listIndex + 1);
			std::iter_swap(m_fieldsCategoryMaps.begin() + listIndex, m_fieldsCategoryMaps.begin() + listIndex + 1);
			BuildItemLabel(listIndex);
			BuildItemLabel(listIndex + 1);
			unsavedChanges = true;
		}
		ImGui::SameLine();
	}

	// Don't show delete button on last item
	if (listIndex + 1 != m_listSize || m_listSize == 1)
	{
		ImGui::SetCursorPosX(pos_x + buttonWidth * 3);
		if (ImGui::Button("X", buttonSize))
		{
			// Delete this item
			--m_listSizeChange;
			--m_listSize;

			// Only delete if we're not at the list end, to avoid destructing the fields before using them
			// Reducing list size will make things invisible on the next render anyway
			if (listIndex < m_listSize) {
				// Shift following items up
				m_fieldIdentifierMaps.erase(m_fieldIdentifierMaps.begin() + listIndex);
				m_fieldsCategoryMaps.erase(m_fieldsCategoryMaps.begin() + listIndex);
				m_itemLabels.erase(m_itemLabels.begin() + listIndex);
				unsavedChanges = true;
				
				// Rebuild labels
				for (int i = listIndex; i < m_listSize; ++i) {
					BuildItemLabel(i);
				}
			}
		}
		ImGui::SameLine();
	}


	ImGui::PopID();
	ImGui::PopID();
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
			for (uint32_t listIndex = 0; listIndex < m_listSize; ++listIndex)
			{
				if (id > 1) {
					// Only non-starting lists may have access to list controls
					RenderListControlButtons(listIndex);
				}

				ImGui::PushID(listIndex);
				if (!ImGui::TreeNode(this + listIndex, m_itemLabels[listIndex].c_str())) {
					// Tree node hidden so no need to render anything
					ImGui::PopID();
					continue;
				}
				ImGui::PopID();

				// Responsive form that tries to use big widths to draw up to 4 fields (+ 4 labels) per line
				for (uint8_t category : m_categories)
				{
					const int headerFlags = ImGuiTreeNodeFlags_Framed | (category & 1 ? 0 : ImGuiTreeNodeFlags_DefaultOpen);
					if (category != 0 && !ImGui::CollapsingHeader(_(m_categoryStringIdentifiers[category].c_str()), headerFlags)) {
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

void EditorFormList::BuildItemLabel(int listIdx)
{
	std::string label = std::format("{} {} ({})", _(std::format("{}.window_name", m_identifierPrefix).c_str()), listIdx, listIdx + id);

	m_itemLabels[listIdx] = label;
}