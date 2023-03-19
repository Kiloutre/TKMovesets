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
		
		m_editor->SaveItem(windowType, id + listIndex, m_items[listIndex]->identifierMaps);
	}

	OnApply();
}

bool EditorFormList::IsFormValid()
{
	for (uint32_t listIndex = 0; listIndex < m_listSize; ++listIndex)
	{
		for (auto& [category, fields] : m_items[listIndex]->categoryMaps) {
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
	const char* fieldLabel = _(field->displayName.c_str());
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
	auto fieldIdentifierMaps = editor->GetFormFieldsList(windowType, t_id, drawOrder);
	m_listSize = fieldIdentifierMaps.size();

	// Tries to find a name to show in the window title
	// Also figure out the categories
	char name[32] = "";
	for (const std::string& fieldName : drawOrder) {
		EditorInput* field = fieldIdentifierMaps[0][fieldName];
		m_categories.insert(field->category);

		// Moves are the only named fields so there is no reason to write any more complex code for now
		if (fieldName == "move_name") {
			strcpy_s(name, sizeof(name), field->buffer);
		}
	}

	// Builds the <category : fields> maps & the item labels
	for (uint32_t listIndex = 0; listIndex < m_listSize; ++listIndex)
	{
		m_items.push_back(new FieldItem);
		auto& item = m_items[listIndex];

		item->identifierMaps = fieldIdentifierMaps[listIndex];

		for (uint8_t category : m_categories)
		{
			std::vector<EditorInput*> inputs;
			for (const std::string& fieldName : drawOrder) {
				EditorInput* field = fieldIdentifierMaps[listIndex][fieldName];
				if (field->category == category) {
					inputs.push_back(field);
				}
			}
			item->categoryMaps[category] = inputs;
		}

		BuildItemDetails(listIndex);
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
	float pos_x = ImGui::GetContentRegionAvail().x + 15 - buttonWidth * 4;

	if (pos_x <= 220) {
		// If too little place, just don't draw controls (for now)
		return;
	}

	ImGui::PushID(this);
	ImGui::PushID(listIndex);

	// No point in having the + on every single item
	if (listIndex + 1 != m_listSize || m_listSize == 1) {
		ImGui::SetCursorPosX(pos_x);
		if (ImGui::Button("+", buttonSize)) {
			VectorSet<std::string> drawOrder;

			m_items.insert(m_items.begin() + listIndex, new FieldItem);
			auto& item = m_items[listIndex];

			item->identifierMaps = m_editor->GetFormFieldsList(windowType, 0, drawOrder)[0];
			item->openStatus = EditorFormTreeview_ForceOpen;

			for (uint8_t category : m_categories)
			{
				std::vector<EditorInput*> inputs;
				for (const std::string& fieldName : drawOrder) {
					EditorInput* field = item->identifierMaps[fieldName];
					if (field->category == category) {
						inputs.push_back(field);
					}
				}
				item->categoryMaps[category] = inputs;
			}

			++m_listSizeChange;
			++m_listSize;

			for (int i = listIndex; i < m_listSize; ++i) {
				BuildItemDetails(i);
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
			std::iter_swap(m_items.begin() + listIndex, m_items.begin() + listIndex - 1);

			// Ensure that whatever is open/closed will stay that way after being moved. TreeNode are annoying like that.
			if (m_items[listIndex]->openStatus == EditorFormTreeview_Opened) {
				m_items[listIndex]->openStatus = EditorFormTreeview_ForceOpen;
			} else if (m_items[listIndex]->openStatus == EditorFormTreeview_Closed) {
				m_items[listIndex]->openStatus = EditorFormTreeview_ForceClose;
			}

			if (m_items[listIndex - 1]->openStatus == EditorFormTreeview_Opened) {
				m_items[listIndex - 1]->openStatus = EditorFormTreeview_ForceOpen;
			}
			else if (m_items[listIndex - 1]->openStatus == EditorFormTreeview_Closed) {
				m_items[listIndex - 1]->openStatus = EditorFormTreeview_ForceClose;
			}

			BuildItemDetails(listIndex);
			BuildItemDetails(listIndex - 1);
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
			std::iter_swap(m_items.begin() + listIndex, m_items.begin() + listIndex + 1);

			// Ensure that whatever is open/closed will stay that way after being moved. TreeNode are annoying like that.
			if (m_items[listIndex]->openStatus == EditorFormTreeview_Opened) {
				m_items[listIndex]->openStatus = EditorFormTreeview_ForceOpen;
			}
			else if (m_items[listIndex]->openStatus == EditorFormTreeview_Closed) {
				m_items[listIndex]->openStatus = EditorFormTreeview_ForceClose;
			}

			if (m_items[listIndex + 1]->openStatus == EditorFormTreeview_Opened) {
				m_items[listIndex + 1]->openStatus = EditorFormTreeview_ForceOpen;
			}
			else if (m_items[listIndex + 1]->openStatus == EditorFormTreeview_Closed) {
				m_items[listIndex + 1]->openStatus = EditorFormTreeview_ForceClose;
			}

			BuildItemDetails(listIndex);
			BuildItemDetails(listIndex + 1);
			unsavedChanges = true;
		}
		ImGui::SameLine();
	}

	// Don't show delete button on last item
	if (listIndex + 1 != m_listSize || m_listSize == 1)
	{
		ImGui::SetCursorPosX(pos_x + buttonWidth * 3);
		ImGui::PushStyleColor(ImGuiCol_Button, FORM_DELETE_BTN);
		if (ImGui::Button("X", buttonSize))
		{
			// Delete this item
			--m_listSizeChange;
			--m_listSize;

			// Only delete if we're not at the list end, to avoid destructing the fields before using them
			// Reducing list size will make things invisible on the next render anyway
			if (listIndex < m_listSize) {
				// Shift following items up
				delete m_items[listIndex];
				m_items.erase(m_items.begin() + listIndex);
				unsavedChanges = true;
				
				// Rebuild labels
				for (int i = listIndex; i < m_listSize; ++i) {
					BuildItemDetails(i);
				}
			}
		}
		ImGui::PopStyleColor();
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
		if (m_requestedClosure) {
			RenderDiscardButtons();
		}
		else
		{
			currentViewport = ImGui::GetWindowViewport();

			// Responsive form that tries to use big widths to draw up to 4 fields (+ 4 labels) per line
			const int columnCount = EditorFormUtils::GetColumnCount();
			const float drawWidth = ImGui::GetContentRegionAvail().x;
			ImDrawList* drawlist = ImGui::GetWindowDrawList();
			const ImVec2 c_winPos = ImGui::GetWindowPos();

			for (uint32_t listIndex = 0; listIndex < m_listSize; ++listIndex)
			{
				{
					ImVec2 drawStart = c_winPos + ImGui::GetCursorPos();
					drawStart.y -= ImGui::GetScrollY() + 2;
					drawlist->AddRectFilled(drawStart, drawStart + ImVec2(drawWidth, ImGui::GetTextLineHeightWithSpacing() + 4), listIndex & 1 ? FORM_BG_1 : FORM_BG_2);
				}

				if (id > 1) {
					// Only non-starting lists may have access to list controls
					RenderListControlButtons(listIndex);
				}

				auto& item = m_items[listIndex];

				// This allows us to force some treenode to open at certain times, without forcing them to be closed
				auto openStatus = item->openStatus;
				if (openStatus == EditorFormTreeview_ForceOpen || openStatus == EditorFormTreeview_ForceClose) {
					ImGui::SetNextItemOpen(openStatus == EditorFormTreeview_ForceOpen);
				}

				ImGui::PushID(listIndex);
				if (!ImGui::TreeNodeExV(this + listIndex, ImGuiTreeNodeFlags_SpanAvailWidth, item->itemLabel.c_str(), va_list())) {
					// Tree node hidden so no need to render anything
					ImGui::PopID();
					item->openStatus = EditorFormTreeview_Closed;
					continue;
				}
				ImGui::PopID();
				item->openStatus = EditorFormTreeview_Opened;

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
						std::vector<EditorInput*>& inputs = item->categoryMaps[category];
						RenderInputs(listIndex, inputs, category, columnCount);
						ImGui::EndTable();
					}

				}

				ImGui::TreePop();
			}

			bool enabledBtn = unsavedChanges;
			if (enabledBtn) {
				ImGui::PushStyleColor(ImGuiCol_Button, FORM_SAVE_BTN);
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

void EditorFormList::BuildItemDetails(int listIdx)
{
	std::string label = std::format("{} {} ({})", _(std::format("{}.window_name", m_identifierPrefix).c_str()), listIdx, listIdx + id);

	m_items[listIdx]->itemLabel = label;
}