#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorFormList.hpp"
#include "Localization.hpp"
#include "EditorVisuals.hpp"
#include "EditorClipboard.hpp"
#include <memory>

// -- Public methods -- //

EditorFormList::EditorFormList(const std::string& parentWindowName, EditorWindowType t_windowType, uint32_t t_structureId, EditorLogic* editor, EditorVisuals* baseWindow, int listSize, const char* typeName)
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
    EditList([&] {
        FlushPendingFields();
        if (!IsFormValid()) return;
        const int sizeChange = m_listSizeChange;
        const int oldSize = static_cast<int>(GetOriginalStructureListSize());
        bool reordered = false;
        std::vector<int> ids;
        for (size_t i = 0; i < m_items.size(); ++i) {
            ids.push_back(m_items[i]->id);
            reordered |= m_items[i]->id != structureId + i;
        }
        if (sizeChange || reordered)
            m_editor->ModifyListSize(windowType, structureId, ids, m_deletedItemIds);
        for (uint32_t i = 0; i < m_listSize; ++i)
            m_editor->SaveItemWide(windowType, structureId + i, m_items[i]->identifierMap);

        // Observer and finalization callbacks must finish before committing editor bytes.
        if (sizeChange) OnApplyResize(sizeChange, oldSize);
        m_listSizeChange = 0;
        for (size_t i = 0; i < m_items.size(); ++i) m_items[i]->id = structureId + i;
        m_deletedItemIds.clear();
        unsavedChanges = false;
        justAppliedChanges = true;
        m_requestedClosure = false;
        if (!m_listSize) popen = false;
        OnApply();
    });
}

void EditorFormList::CopyFormToClipboard() const
{
	std::string clipboardText = m_windowTypeName + "\n";

	for (auto& item : m_items)
	{
		for (auto& [fieldName, field] : item->identifierMap)
		{
			//DEBUG_LOG("C: [%s] = [%s]\n", fieldName.c_str(), field->buffer);
			clipboardText += field->nextValue ? *field->nextValue : field->buffer;
			clipboardText += "\n";
		}
	}
	ImGui::SetClipboardText(clipboardText.c_str());
}

namespace {
using RowOwner = std::unique_ptr<FieldItem>;
RowOwner CloneRow(const FieldItem& source, size_t& budget) {
    RowOwner row(new FieldItem);
    row->itemLabel = source.itemLabel; row->openStatus = source.openStatus;
    row->color = source.color; row->id = source.id;
    row->identifierMap = EditorFormUtils::CloneFields(source.identifierMap, budget);
    row->categoryMap = EditorFormUtils::CloneCategories(source.identifierMap, row->identifierMap, source.categoryMap);
    return row;
}
}

std::vector<size_t> EditorFormList::ClipboardWidths() const
{
    std::vector<size_t> widths;
    if (!m_items.empty()) for (const auto& [name, field] : m_items.front()->identifierMap)
        widths.push_back(field && field->buffer ? field->bufsize : 0);
    return widths;
}

void EditorFormList::PasteText(std::string_view text)
{
    const auto widths = ClipboardWidths();
    auto rows = EditorClipboard::Decode(text, m_windowTypeName, widths, ClipboardRowLimit());
    if (m_items.empty() || m_listSize != m_items.size() || m_items.size() > ClipboardRowLimit()) throw std::invalid_argument("Invalid destination list size.");
    EditList([&] {
        size_t budget = 64 * 1024 * 1024;
        for (const auto& row : m_items)
            for (const auto& [name, field] : row->identifierMap) {
                if (field->bufsize > budget || sizeof(EditorInput) > budget - field->bufsize)
                    throw std::length_error("Clipboard form exceeds its 64 MiB field-storage budget.");
                budget -= field->bufsize + sizeof(EditorInput);
            }
        const auto oldSize = m_listSize;
        while (m_items.size() < rows.size()) {
            auto row = CloneRow(*m_items.front(), budget); row->id = -1;
            m_items.insert(m_items.end() - 1, std::move(row));
        }
        for (size_t r = 0; r < rows.size(); ++r) {
            auto& fields = m_items[r]->identifierMap;
            if (fields.size() != widths.size()) throw std::invalid_argument("Clipboard destination schemas differ.");
            auto expected = m_items.front()->identifierMap.begin();
            size_t c = 0;
            for (const auto& [name, field] : fields) {
                if (name.str() != expected->first.str() || field->bufsize != widths[c])
                    throw std::invalid_argument("Clipboard destination schemas differ.");
                field->nextValue = std::move(rows[r][c]);
                ++c; ++expected;
            }
            m_items[r]->openStatus = EditorFormTreeview_ForceOpen;
        }
        m_listSize = m_items.size();
        m_listSizeChange += static_cast<int>(m_listSize - oldSize);
        if (m_listSize != oldSize) OnResize();
        for (size_t r = 0; r < rows.size(); ++r) BuildItemDetails(static_cast<int>(r));
        moveFocusAway = true; unsavedChanges = true;
    });
}

void EditorFormList::FlushPendingFields()
{
    for (size_t row = 0; row < m_items.size(); ++row)
        for (auto& [name, field] : m_items[row]->identifierMap) {
            if (field->ConsumeNextValue()) {
                unsavedChanges = true;
                field->errored = !m_editor->ValidateField(windowType, field);
                if (!field->errored) OnUpdate(static_cast<int>(row), field);
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
            TryListEdit([&] { CreateNewItem(listIndex); });
		}
		ImGui::SameLine();
	}

	// Don't allow first item and ending item to shift up
	if (listIndex != 0 && listIndex + 1 != m_listSize) {
		ImGui::SetCursorPosX(pos_x + buttonWidth);
		if (ImGui::Button("^", buttonSize))
		{
            TryListEdit([&] { MoveItem(listIndex, listIndex - 1); });
		}
		ImGui::SameLine();
	}

	// Don't allow last 2 items to shift down
	if (listIndex + 2 < m_listSize) {
		ImGui::SetCursorPosX(pos_x + buttonWidth * 2);
		if (ImGui::Button("V", buttonSize))
		{
            TryListEdit([&] { MoveItem(listIndex, listIndex + 1); });
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
            TryListEdit([&] { RemoveItem(listIndex); });
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

    if (m_items.empty() || insertionPosition < 0 || static_cast<size_t>(insertionPosition) >= m_items.size() || m_items.size() >= ClipboardRowLimit())
        throw std::out_of_range("Cannot insert a row at this position.");
    if (copyFormId < 0 || static_cast<size_t>(copyFormId) >= m_items.size())
        throw std::out_of_range("Cannot copy a row from this position.");
    EditList([&] {
        // Prefill from the form row being copied (its current values), not from the editor at structureId + form index:
        // after unapplied inserts a form index can lie past the stored list, and the editor would read beyond its table.
        size_t budget = 64 * 1024 * 1024;
        RowOwner prepared = CloneRow(*m_items[copyFormId], budget);
        auto* item = prepared.get();
        item->id = -1;
        item->color = 0;
        item->openStatus = EditorFormTreeview_ForceOpen;
        for (const auto& [name, field] : item->identifierMap) field->savedText.reset(); // A new row has no applied value

        m_items.insert(m_items.begin() + insertionPosition, std::move(prepared));
        ++m_listSizeChange; ++m_listSize;
        BuildItemDetails(insertionPosition);
        OnResize();
        unsavedChanges = true;
    });
	return (unsigned int)insertionPosition;
}

bool EditorFormList::CanDuplicateItemToEnd(size_t source) const
{
	// The last row ends the list; the starting lists' first row has no list controls either (RenderListControlButtons).
	return m_items.size() > 1 && source + 1 < m_items.size() && !(structureId <= 1 && source == 0 && !uniqueType) &&
		m_items.size() < ClipboardRowLimit();
}

unsigned int EditorFormList::DuplicateItemToEnd(size_t source)
{
	if (!CanDuplicateItemToEnd(source))
		throw std::out_of_range("This row cannot be duplicated to the end of the list.");
	const int position = static_cast<int>(m_items.size()) - 1;
	EditList([&] {
		size_t budget = 64 * 1024 * 1024;
		RowOwner prepared = CloneRow(*m_items[source], budget);
		auto* item = prepared.get();
		item->id = -1;
		item->color = 0;
		item->openStatus = EditorFormTreeview_ForceOpen;
		for (const auto& [name, field] : item->identifierMap) field->savedText.reset(); // A new row has no applied value

		m_items.insert(m_items.begin() + position, std::move(prepared));
		++m_listSizeChange; ++m_listSize;
		BuildItemDetails(position);
		BuildItemDetails(position + 1); // the end row moved down one index
		OnResize();
		unsavedChanges = true;
	});
	return static_cast<unsigned int>(position);
}

void EditorFormList::RenderItemContextMenuItems()
{
	if (m_contextItem < 0 || static_cast<size_t>(m_contextItem) >= m_items.size()) return;
	const auto row = static_cast<size_t>(m_contextItem);
	const auto label = std::format("{} ({})", _("edition.form_popup.duplicate_item"), m_items[row]->itemLabel);
	ImGuiExtra::DisableBlockIf __(!CanDuplicateItemToEnd(row));
	if (ImGui::Selectable(label.c_str(), false, 0, ImVec2(0, ImGui::GetFrameHeightWithSpacing())))
	{
		TryListEdit([&] { DuplicateItemToEnd(row); });
		ImGui::CloseCurrentPopup();
	}
}

void EditorFormList::DeleteStructure()
{
    EditList([&] {
        auto origSize = GetOriginalStructureListSize();
        m_editor->DeleteStructures(windowType, structureId, origSize);
        popen = false; unsavedChanges = false;
        OnApplyResize(-origSize, origSize);
    });
}

void EditorFormList::InitForm(std::string windowTitleBase, uint32_t t_id, EditorLogic* editor)
{
	structureId = t_id;
	m_editor = editor;
    if (!editor) throw std::invalid_argument("Cannot construct a form without an editor.");
    if (m_listSize > ClipboardRowLimit()) throw std::out_of_range("Editor list exceeds its record range.");

	VectorSet<std::string> drawOrder;

	std::vector<InputMap> fieldIdentifierMaps;

	if (m_listSize == 0) {
		fieldIdentifierMaps = editor->GetFormFieldsListWide(windowType, t_id, drawOrder,0);
		m_listSize = fieldIdentifierMaps.size();
		DEBUG_LOG("GetFormFieldsList() without list size, returned size is %llu\n", m_listSize);
	} else {
		// Some struct lists may have fixed list sizes: in this case, we'll call a different function
		// For now these two GetFormFields aren't swappable, if a list is not supposed to have a known size ahead of time, don't provide a size
		fieldIdentifierMaps = editor->GetFormFieldsListWide(windowType, t_id, drawOrder, (int)m_listSize);
		DEBUG_LOG("GetFormFieldsList() with list size %llu, returned size is %llu\n", m_listSize, fieldIdentifierMaps.size());
	}

    if (fieldIdentifierMaps.empty() || fieldIdentifierMaps.size() != m_listSize ||
        m_listSize > ClipboardRowLimit())
        throw std::invalid_argument("Editor field factory returned an invalid row count.");
    for (const auto& fields : fieldIdentifierMaps) EditorFormUtils::ValidateFields(fields, drawOrder);
	DEBUG_LOG("GetFormFieldsList: drawOrder size is %llu\n", drawOrder.size());

	// Tries to find a name to show in the window title
	// Also figure out the categories


	for (const std::string& fieldName : drawOrder) {
		EditorInput* field = fieldIdentifierMaps.at(0).at(fieldName);
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
		m_items.push_back(std::make_unique<FieldItem>());
		auto& item = m_items[listIndex];

		item->id = structureId + listIndex;
		item->identifierMap = std::move(fieldIdentifierMaps[listIndex]);

		for (uint8_t category : m_categories)
		{
			std::vector<EditorInput*> inputs;
			for (const std::string& fieldName : drawOrder) {
				EditorInput* field = item->identifierMap.at(fieldName);
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

	// Right click: remember the row under the cursor (its header and fields) for the context menu.
	const bool rightClicked = ImGui::GetIO().MouseClicked[1] && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
	const float mouseY = ImGui::GetMousePos().y;
	if (rightClicked) m_contextItem = -1;
	float rowTop = 0;
	auto hitRow = [&](uint32_t row, float bottom) { if (rightClicked && mouseY >= rowTop && mouseY < bottom) m_contextItem = static_cast<int>(row); };

	for (uint32_t listIndex = 0; listIndex < m_listSize; ++listIndex)
	{
		if (listIndex > 0) hitRow(listIndex - 1, ImGui::GetCursorScreenPos().y);
		rowTop = ImGui::GetCursorScreenPos().y - 2;
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
		if (!ImGui::TreeNodeEx(this + listIndex, nodeFlags, "%s", item->itemLabel.c_str())) {
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
	if (m_listSize > 0) hitRow(static_cast<uint32_t>(m_listSize - 1), ImGui::GetCursorScreenPos().y);
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

EditorFormList::~EditorFormList() = default;

struct EditorFormList::SavedListState final : TransactionState {
    std::unique_ptr<TransactionState> base;
    EditorFormList& form;
    std::vector<RowOwner> original;
    std::map<FieldItem*, size_t> identities;
    size_t size;
    int change;
    std::set<int> deleted;
    std::string resizeKey;
    bool committed = false;
    SavedListState(EditorFormList& target, size_t& budget)
        : base(target.EditorForm::BeginTransaction(budget)), form(target), size(target.m_listSize),
          change(target.m_listSizeChange), deleted(target.m_deletedItemIds), resizeKey(target.m_resizeEventKey) {
        std::vector<RowOwner> copy;
        copy.reserve(form.m_items.size());
        for (const auto& row : form.m_items) {
            copy.push_back(CloneRow(*row, budget));
            identities.emplace(copy.back().get(), copy.size() - 1);
        }
        original.swap(form.m_items); form.m_items.swap(copy);
    }
    ~SavedListState() override {
        if (committed) return;
        original.swap(form.m_items);
        form.m_listSize = size; form.m_listSizeChange = change;
        deleted.swap(form.m_deletedItemIds); resizeKey.swap(form.m_resizeEventKey);
    }
    void Commit() noexcept override {

        // Keep surviving row identities on successful insertion/removal/reordering too.
        for (auto& row : form.m_items) {
            auto found = identities.find(row.get());
            if (found == identities.end()) continue;
            auto& previous = original[found->second];
            previous->identifierMap.swap(row->identifierMap);
            previous->categoryMap.swap(row->categoryMap);
            previous->itemLabel.swap(row->itemLabel);
            std::swap(previous->openStatus, row->openStatus);
            std::swap(previous->color, row->color);
            std::swap(previous->id, row->id);
            previous.swap(row);
        }
        base->Commit(); committed = true;
    }
};
std::unique_ptr<EditorForm::TransactionState> EditorFormList::BeginTransaction(size_t& budget) {
    return std::make_unique<SavedListState>(*this, budget);
}
void EditorFormList::EditList(const std::function<void()>& action) {
    EditForm([&] { action(); return true; });
}
void EditorFormList::TryListEdit(const std::function<void()>& action) {
    try { action(); m_operationError.clear(); }
    catch (const std::exception& error) { m_operationError = error.what(); }
    catch (...) { m_operationError = "The list edit failed."; }
}
void EditorFormList::RemoveItem(size_t index) {
    if (index >= m_items.size() || (index + 1 == m_items.size() && m_items.size() != 1) ||
        (uniqueType && m_items.size() == 1) || (structureId <= 1 && index == 0 && !uniqueType))
        throw std::out_of_range("Cannot remove the retained last row or protected starting row.");
    EditList([&] {
        if (m_items[index]->id != -1) m_deletedItemIds.insert(m_items[index]->id);
        m_items.erase(m_items.begin() + index);
        --m_listSize; --m_listSizeChange;
        unsavedChanges = true;
        OnResize();
    });
}
void EditorFormList::MoveItem(size_t from, size_t to) {
    if (from >= m_items.size() || to >= m_items.size() || from + 1 == m_items.size() || to + 1 == m_items.size() ||
        (from > to ? from - to : to - from) != 1)
        throw std::out_of_range("Only adjacent rows before the retained last row can be reordered.");
    EditList([&] {
        std::swap(m_items[from], m_items[to]);
        for (auto index : {from, to}) {
            auto& status = m_items[index]->openStatus;
            if (status == EditorFormTreeview_Opened) status = EditorFormTreeview_ForceOpen;
            else if (status == EditorFormTreeview_Closed) status = EditorFormTreeview_ForceClose;
        }
        unsavedChanges = true;
        OnReorder();
    });
}

std::unique_ptr<EditorForm::TransactionState> EditorFormList::BeginListTransaction() {
    if (m_baseWindow) return m_baseWindow->BeginFormTransaction(*this);
    size_t budget = 64 * 1024 * 1024;
    return BeginTransaction(budget);
}

std::unique_ptr<EditorForm::TransactionState> EditorFormList::BeginEditTransaction() {
    return BeginListTransaction();
}
