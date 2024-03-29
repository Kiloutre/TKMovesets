#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorFormList.hpp"
#include "Localization.hpp"
#include "EditorVisuals.hpp"

// -- Public methods -- //

EditorFormList::EditorFormList(const std::string& parentWindowName, EditorWindowType t_windowType, uint16_t t_structureId, EditorLogic* editor, EditorVisuals* baseWindow, int listSize, const char* typeName)
{
	m_windowTypeName = typeName;
	windowType = t_windowType;
	m_baseWindow = baseWindow;
	m_listSize = listSize;
	InitForm(parentWindowName, t_structureId, editor);
    m_isDeletable = false;
}

void EditorFormList::OnInitEnd()
{
	for (unsigned int i = 0; i < m_listSize; ++i) {
		BuildItemDetails(i);
	}
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
		int newSize = (int)m_listSize;
		int oldSize = (int)GetOriginalStructureListSize();
		std::vector<int> itemIndexes;
		for (auto& item : m_items) {
			itemIndexes.push_back(item->id);
		}
		m_editor->ModifyListSize(windowType, structureId, itemIndexes, m_deletedItemIds);
		OnApplyResize(m_listSizeChange, oldSize);
		m_listSizeChange = 0;
	}
	else {
		// Could optimize thing if needed here by calling a struct reordering function
		// At it stands this isn't really needed and worth the effort at the current time
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
		m_editor->SaveItem(windowType, structureId + listIndex, m_items[listIndex]->identifierMap);
		// Re-set the IDs of the list items in case reordering/deletion/creation happened
		m_items[listIndex]->id = structureId + listIndex;
	}

	m_deletedItemIds.clear();

	OnApply();
}

void EditorFormList::CopyFormToClipboard() const
{
	std::string clipboardText = m_windowTypeName + "\n";

	for (auto& item : m_items)
	{
		for (auto& [fieldName, field] : item->identifierMap)
		{
			//DEBUG_LOG("C: [%s] = [%s]\n", fieldName.c_str(), field->buffer);
			clipboardText += field->buffer;
			clipboardText += "\n";
		}
	}
	ImGui::SetClipboardText(clipboardText.c_str());
}

void EditorFormList::PasteFormFromClipboard()
{
	moveFocusAway = true;
	const char* clipboardText = ImGui::GetClipboardText();
	while (*clipboardText != '\n') ++clipboardText;
	++clipboardText;


	{
		// Count the amount of fields, if there are too many fields to fit in the current list, make its size grow
		unsigned int expectedFieldAmount = 0;
		const char* cursor = clipboardText;
		while (*cursor != '\0')
		{
			if (*cursor == '\n') {
				++expectedFieldAmount;
			}
			++cursor;
		}

		unsigned int fieldAmountPerItem = (unsigned int)m_items[0]->identifierMap.size();
		unsigned int fieldAmount = fieldAmountPerItem * (unsigned int)m_items.size();

		while (fieldAmount < expectedFieldAmount) {
			auto newId = CreateNewItem(-1);
			m_items[newId]->openStatus = EditorFormTreeview_ForceOpen;
			fieldAmount += fieldAmountPerItem;
		}
	}

	for (auto& item : m_items)
	{
		for (auto& [fieldName, field] : item->identifierMap)
		{
			const char* newlinePos = strstr(clipboardText, "\n");

			if (newlinePos == nullptr) {
				newlinePos = (clipboardText + strlen(clipboardText));
				return;
			}

			item->openStatus = EditorFormTreeview_ForceOpen;
			field->nextValue = std::string(clipboardText, newlinePos - clipboardText);
			//DEBUG_LOG("P: [%s] -> [%s]\n", fieldName.c_str(), field->nextValue.c_str());
			clipboardText = newlinePos + 1;

			if (*newlinePos == '\0') {
				return;
			}
		}
	}
}

bool EditorFormList::IsFormValid() const
{
	for (uint32_t listIndex = 0; listIndex < m_listSize; ++listIndex)
	{
		for (auto& [category, fields] : m_items[listIndex]->categoryMap) {
			for (auto& field : fields) {
				if (field->errored) {
					return false;
				}
			}
		}
	}
	return true;
}

void EditorFormList::BuildItemDetails(int listIdx)
{
	std::string label = std::format("{} {} ({})", _(std::format("edition.{}.window_name", m_windowTypeName).c_str()), listIdx, listIdx + structureId);

	m_items[listIdx]->itemLabel = label;
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

	if (structureId <= 1 && listIndex == 0 && !uniqueType) {
		ImGui::SetCursorPosX(pos_x);
		ImGuiExtra::HelpMarker(_("edition.form_list.controls_disabled_explanation"));
		ImGui::SetCursorPos(cursor);
		// Only non-starting lists may have access to list controls
		return;
	}

	ImGui::PushID(this);
	ImGui::PushID(listIndex);

	// No point in having the + on every single item
	if (listIndex + 1 != m_listSize || m_listSize == 1) {
		ImGui::SetCursorPosX(pos_x);
		if (ImGui::Button("+", buttonSize)) {
			CreateNewItem(listIndex);
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
			}
			else if (m_items[listIndex]->openStatus == EditorFormTreeview_Closed) {
				m_items[listIndex]->openStatus = EditorFormTreeview_ForceClose;
			}

			if (m_items[listIndex - 1]->openStatus == EditorFormTreeview_Opened) {
				m_items[listIndex - 1]->openStatus = EditorFormTreeview_ForceOpen;
			}
			else if (m_items[listIndex - 1]->openStatus == EditorFormTreeview_Closed) {
				m_items[listIndex - 1]->openStatus = EditorFormTreeview_ForceClose;
			}

			unsavedChanges = true;
			OnReorder();
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

			unsavedChanges = true;
			OnReorder();
		}
		ImGui::SameLine();
	}

	// Don't show delete button on last item
	if ((listIndex + 1 != m_listSize || m_listSize == 1) && (!uniqueType || m_listSize > 1))
	{
		ImGui::SetCursorPosX(pos_x + buttonWidth * 3);
		ImGui::PushStyleColor(ImGuiCol_Button, FORM_DELETE_BTN);
		if (ImGui::Button("X", buttonSize))
		{
			// Delete this item
			--m_listSizeChange;
			--m_listSize;

			
			if (m_items[listIndex]->id != -1) {
				m_deletedItemIds.insert(m_items[listIndex]->id);
			}

			for (auto& [key, fieldPtr] : m_items[listIndex]->identifierMap) {
				delete[] fieldPtr->buffer;
				delete fieldPtr;
			}
			delete m_items[listIndex];
			m_items.erase(m_items.begin() + listIndex);
			unsavedChanges = true;

			OnResize();
		}
		ImGui::PopStyleColor();
		ImGui::SameLine();
	}


	ImGui::PopID();
	ImGui::PopID();
	ImGui::SetCursorPos(cursor);
}

unsigned int EditorFormList::CreateNewItem(int insertionPosition)
{
	int copyFormId = insertionPosition;
	if (insertionPosition <= -1) {
		insertionPosition = (int)m_items.size() - 1;
		copyFormId = 0;
	}

	VectorSet<std::string> drawOrder;

	m_items.insert(m_items.begin() + insertionPosition, new FieldItem);
	auto& item = m_items[insertionPosition];

	item->identifierMap = m_editor->GetListSingleForm(windowType, structureId + copyFormId, drawOrder);
	item->openStatus = EditorFormTreeview_ForceOpen;

	for (uint8_t category : m_categories)
	{
		std::vector<EditorInput*> inputs;
		for (const std::string& fieldName : drawOrder) {
			EditorInput* field = item->identifierMap[fieldName];
			EditorFormUtils::SetFieldDisplayText(field, _(field->fullName.c_str()));
			if (field->category == category) {
				inputs.push_back(field);
			}
		}
		item->categoryMap[category] = inputs;
	}
	BuildItemDetails(insertionPosition);

	for (const std::string& fieldName : drawOrder) {
		EditorInput* field = item->identifierMap[fieldName];
		std::string descriptionKey = field->fullName + ".description";
		field->hoverDescription = Localization::HasText(descriptionKey) ? _(descriptionKey.c_str()) : _(field->fullName.c_str());
		m_categories.insert(field->category);
	}

	++m_listSizeChange;
	++m_listSize;

	unsavedChanges = true;
	OnResize();
	return (unsigned int)insertionPosition;
}

void EditorFormList::DeleteStructure()
{
	auto origSize = GetOriginalStructureListSize();
	m_editor->DeleteStructures(windowType, structureId, origSize);
	popen = false;
	unsavedChanges = false;
	OnApplyResize(-origSize, origSize);
}

void EditorFormList::InitForm(std::string windowTitleBase, uint32_t t_id, EditorLogic* editor)
{
	structureId = t_id;
	m_editor = editor;

	VectorSet<std::string> drawOrder;

	std::vector<InputMap> fieldIdentifierMaps;

	if (m_listSize == 0) {
		fieldIdentifierMaps = editor->GetFormFieldsList(windowType, t_id, drawOrder);
		m_listSize = fieldIdentifierMaps.size();
		DEBUG_LOG("GetFormFieldsList() without list size, returned size is %llu\n", m_listSize);
	} else {
		// Some struct lists may have fixed list sizes: in this case, we'll call a different function
		// For now these two GetFormFields aren't swappable, if a list is not supposed to have a known size ahead of time, don't provide a size
		fieldIdentifierMaps = editor->GetFormFieldsList(windowType, t_id, drawOrder, (int)m_listSize);
		DEBUG_LOG("GetFormFieldsList() with list size %llu, returned size is %llu\n", m_listSize, fieldIdentifierMaps.size());
	}

	DEBUG_LOG("GetFormFieldsList: drawOrder size is %llu\n", drawOrder.size());

	// Tries to find a name to show in the window title
	// Also figure out the categories


	for (const std::string& fieldName : drawOrder) {
		EditorInput* field = fieldIdentifierMaps[0][fieldName];
		m_categories.insert(field->category);
	}

#ifdef BUILD_TYPE_DEBUG
	std::string categories;
	for (auto c : m_categories) {
		categories += std::to_string(c) + " ";
	}
	DEBUG_LOG("Registered categories: %s\n", categories.c_str());
#endif

	// Builds the <category : fields> maps & the item labels
	for (uint32_t listIndex = 0; listIndex < m_listSize; ++listIndex)
	{
		m_items.push_back(new FieldItem);
		auto& item = m_items[listIndex];

		item->id = structureId + listIndex;
		item->identifierMap = fieldIdentifierMaps[listIndex];

		for (uint8_t category : m_categories)
		{
			std::vector<EditorInput*> inputs;
			for (const std::string& fieldName : drawOrder) {
				EditorInput* field = fieldIdentifierMaps[listIndex][fieldName];
				std::string descriptionKey = field->fullName + ".description";
				field->hoverDescription = Localization::HasText(descriptionKey) ? _(descriptionKey.c_str()) : _(field->fullName.c_str());
				EditorFormUtils::SetFieldDisplayText(field, _(field->fullName.c_str()));
				if (field->category == category) {
					inputs.push_back(field);
				}
			}
			item->categoryMap[category] = inputs;
		}

		BuildItemDetails(listIndex);
	}

	// Build category names
	for (uint8_t category : m_categories) {
		m_categoryStringIdentifiers[category] = std::format("edition.{}.category_{}", m_windowTypeName, category);
	}

	m_windowTitleBase = windowTitleBase;
	ApplyWindowName();

}

void EditorFormList::RenderInternal()
{
	currentViewport = ImGui::GetWindowViewport();

	// Responsive form that tries to use big widths to draw up to 4 fields (+ 4 labels) per line
	const int columnCount = EditorFormUtils::GetColumnCount();
	m_labelWidthHalf = m_winInfo.size.x / columnCount / 2;

	const float drawWidth = ImGui::GetContentRegionAvail().x;
	ImDrawList* drawlist = ImGui::GetWindowDrawList();

	for (uint32_t listIndex = 0; listIndex < m_listSize; ++listIndex)
	{
		{
			ImVec2 drawStart = m_winInfo.pos + ImGui::GetCursorPos();
			drawStart.y -= ImGui::GetScrollY() + 2;
			int color = m_items[listIndex]->color;
			color = color == 0 ? (listIndex & 1 ? FORM_BG_1 : FORM_BG_2) : color;
			drawlist->AddRectFilled(drawStart, drawStart + ImVec2(drawWidth, ImGui::GetTextLineHeightWithSpacing() + 4), color);
		}

		PreItemRender(listIndex);
		RenderListControlButtons(listIndex);

		if (listIndex >= m_listSize) break;
		auto& item = m_items[listIndex];

		// This allows us to force some treenode to open at certain times, without forcing them to be closed
		auto openStatus = item->openStatus;
		if (openStatus == EditorFormTreeview_ForceOpen || openStatus == EditorFormTreeview_ForceClose) {
			ImGui::SetNextItemOpen(openStatus == EditorFormTreeview_ForceOpen);
		}

		// Node styling
		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_SpanAvailWidth;
		if (item->id == -1) {
			nodeFlags |= ImGuiTreeNodeFlags_Framed;
			ImGui::PushStyleColor(ImGuiCol_Header, FORM_LIST_NEW_ITEM);
		}

		ImGui::PushID(listIndex);
		if (!ImGui::TreeNodeEx(this + listIndex, nodeFlags, item->itemLabel.c_str())) {
			// Tree node hidden so no need to render anything
			ImGui::PopID();
			item->openStatus = EditorFormTreeview_Closed;
			if (item->id == -1) {
				ImGui::PopStyleColor();
			}
			continue;
		}
		ImGui::PopID();
		item->openStatus = EditorFormTreeview_Opened;
		if (item->id == -1) {
			ImGui::PopStyleColor();
		}

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
				std::vector<EditorInput*>& inputs = item->categoryMap[category];
				RenderInputs(listIndex, inputs, category, columnCount);
				ImGui::EndTable();
			}

		}

		ImGui::TreePop();
	}
}

void EditorFormList::OnResize()
{
	m_editor->live_loadedMoveset = 0;
}

void EditorFormList::OnReorder()
{
	if (m_editor->live_loadedMoveset == 0) {
		return;
	}

	for (int listIdx = 0; listIdx < m_listSize; ++listIdx) {
		for (auto& [key, field] : m_items[listIdx]->identifierMap)
		{
			m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
		}
	}
}

void EditorFormList::OnApplyResize(int sizeChange, int oldSize)
{
	DEBUG_LOG("EditorFormList::OnApplyResize\n");
	m_baseWindow->IssueFieldUpdate(windowType, sizeChange, structureId, structureId + oldSize);
}

void EditorFormList::RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd)
{
	if (winType == windowType){
		// If a struct was created before this one, we must shift our own ID
		if (MUST_SHIFT_ID((int)structureId, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
			structureId += valueChange;
			ApplyWindowName();
		}
	}
}

void EditorFormList::OnUpdate(int listIdx, EditorInput* field)
{
	m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
}

EditorFormList::~EditorFormList()
{
	for (auto& item : m_items) {
		for (auto& [key, fieldPtr] : item->identifierMap) {
			delete[] fieldPtr->buffer;
			delete fieldPtr;
		}
		delete item;
	}

	m_items.clear();
	m_items.shrink_to_fit();
}