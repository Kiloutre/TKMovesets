#include <ImGui.h>
#include <format>

#include "imgui_extras.hpp"
#include "EditorForm.hpp"
#include "Localization.hpp"
#include "EditorVisuals.hpp"
#include "EditorClipboard.hpp"
#include "Keybinds.hpp"
#include "AppTheme.hpp"
#include "TEditor.hpp"

namespace {
ImU32 FormTabAccent(EditorWindowType type) {
    if(type&0x80000000U)return AppTheme::RecordTab(type&0x7fffffffU);
    switch(type) {
    case TEditorWindowType_Reactions:return AppTheme::RecordTab(0);
    case TEditorWindowType_Requirement:return AppTheme::RecordTab(1);
    case TEditorWindowType_HitCondition:return AppTheme::RecordTab(2);
    case TEditorWindowType_Projectile:return AppTheme::RecordTab(3);
    case TEditorWindowType_Pushback:case TEditorWindowType_PushbackExtradata:return AppTheme::RecordTab(4);
    case TEditorWindowType_Cancel:case TEditorWindowType_GroupedCancel:case TEditorWindowType_CancelExtradata:return AppTheme::RecordTab(6);
    case TEditorWindowType_Extraproperty:case TEditorWindowType_MoveBeginProperty:case TEditorWindowType_MoveEndProperty:return AppTheme::RecordTab(9);
    case TEditorWindowType_Voiceclip:return AppTheme::RecordTab(13);
    case TEditorWindowType_InputSequence:case TEditorWindowType_Input:case TEditorWindowType_MovelistInput:return AppTheme::RecordTab(14);
    case TEditorWindowType_CameraData:case TEditorWindowType_ThrowCamera:return AppTheme::RecordTab(17);
    default:return 0;
    }
}
}

// -- Helpers -- //

namespace EditorFormUtils
{
    InputMap CloneFields(const InputMap& source, size_t& budget) {
        InputMap result;
        for (const auto& [name, original] : source) {
            if (!original || !original->buffer || !original->bufsize || original->bufsize > budget || sizeof(EditorInput) > budget - original->bufsize)
                throw std::length_error("Editor transaction exceeds its 64 MiB field-storage budget.");
            budget -= original->bufsize + sizeof(EditorInput);
            OwnedEditorInput field(new EditorInput(*original));
            field->buffer = nullptr;
            auto buffer = std::make_unique<char[]>(original->bufsize);
            memcpy(buffer.get(), original->buffer, original->bufsize);
            field->buffer = buffer.release();
            result.Own(name.str(), std::move(field));
        }
        return result;
    }
    std::map<int, std::vector<EditorInput*>> CloneCategories(const InputMap& source, const InputMap& copy,
        const std::map<int, std::vector<EditorInput*>>& categories) {
        std::map<const EditorInput*, EditorInput*> pointers;
        for (const auto& [name, original] : source) pointers.emplace(original, copy.at(name));
        std::map<int, std::vector<EditorInput*>> result;
        for (const auto& [category, fields] : categories) {
            auto& copyFields = result[category];
            for (auto* field : fields) copyFields.push_back(pointers.at(field));
        }
        return result;
    }
    void ValidateFields(const InputMap& fields, const VectorSet<std::string>& order) {
        if (fields.empty() || fields.size() != order.size())
            throw std::invalid_argument("Editor field factory returned an incomplete draw order.");
        for (const auto& name : order) {
            const auto* field = fields.at(name);
            if (!field || !field->buffer || !field->bufsize || !memchr(field->buffer, 0, field->bufsize))
                throw std::invalid_argument("Editor field factory returned an invalid field.");
        }
    }

	void SetFieldDisplayText(EditorInput* field, const std::string& newName)
	{
		field->displayName = newName;
		field->textSizeHalf = ImGui::CalcTextSize(newName.c_str()).x / 2;
	}

	int GetColumnCount()
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
}

// -- Public methods -- //

EditorForm::EditorForm(const std::string& parentWindowName, EditorWindowType t_windowType, uint16_t t_structureId, EditorLogic* editor, EditorVisuals* baseWindow, int listSize, const char* typeName)
{
	m_windowTypeName = typeName;
	windowType = t_windowType;
	m_baseWindow = baseWindow;
	InitForm(parentWindowName, t_structureId, editor);
	// Disable single-structure deletion for now.
	m_isDeletable = true;
}

// -- Private methods -- //

void EditorForm::RenderDiscardButtons()
{
	ImGui::TextUnformatted(_("edition.discard_changes"));

	if (ImGui::Button(_("yes")))
	{
		unsavedChanges = false;
		popen = false;
	}
	ImGui::SameLine();
	if (ImGui::Button(_("no"))) {
		m_requestedClosure = false;
	}
}

void EditorForm::Apply()
{
    EditForm([&] {
        FlushPendingFields();
        if (!IsFormValid()) return false;
        m_editor->SaveItem(windowType, structureId, m_fieldIdentifierMap);
        unsavedChanges = false; justAppliedChanges = true; m_requestedClosure = false;
        OnApply();
        return true;
    });
}

void EditorForm::RenderInputs(int listIdx, std::vector<EditorInput*>& inputs, int category, int columnCount)
{
	unsigned int colIdx = 0;
	for (auto& field : inputs)
	{
		if (!field->visible) {
			// If not visible: don't render and don't increment colIdx
			continue;
		}

		//Render label
		if (colIdx % columnCount == 0) {
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

		++colIdx;
	}
}

void EditorForm::RenderLabel(int listIdx, EditorInput* field)
{
	const char* fieldLabel = _(field->displayName.c_str());

	if (field->flags & EditorInput_Interactable && (!field->errored || field->flags & EditorInput_ClickableAlways)) {
		ImGui::PushStyleColor(ImGuiCol_Header, FORM_LABEL_CLICKABLE);

		if (ImGui::Selectable(fieldLabel, true)) {
			OnFieldLabelClick(listIdx, field);
		}
		ImGui::PopStyleColor();
	} else {
		ImVec2 pos = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(pos.x + m_labelWidthHalf - field->textSizeHalf, pos.y + 2));
		ImGui::TextUnformatted(fieldLabel);
		ImGui::SetCursorPos(pos);
	}


	// No tooltip when the field has no description (the retail T8 forms fill it per field).
	if (!field->hoverDescription.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
	{
		ImGui::BeginTooltip();
		ImGui::NewLine();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(field->hoverDescription.c_str());
		ImGui::PopTextWrapPos();
		ImGui::NewLine();
		ImGui::EndTooltip();
	}

}

void EditorForm::RenderInput(int listIdx, EditorInput* field)
{
	bool appliedBg = field->errored;
	if (appliedBg) {
		ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(186, 54, 54, 150));
	}
	else if (field->color != 0) {
		ImGui::PushStyleColor(ImGuiCol_FrameBg, field->color);
		appliedBg = true;
	}

	bool pastingToField = false;
    try { pastingToField = field->ConsumeNextValue(); }
    catch (const std::exception& error) { field->nextValue.reset(); m_operationError = error.what(); }
	if (pastingToField) {
		// Have to implement pasting manually
		// Put the focus back on the input as it should be after pasting
		ImGui::SetKeyboardFocusHere();
	}

	ImGui::PushID(field);
	ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

	if (ImGui::InputText("##", field->buffer, field->bufsize, field->imguiInputFlags)
		|| pastingToField)
	{

		if (m_ignoreNextChange) {
			m_ignoreNextChange = false;
		}
		else {
			unsavedChanges = true;
			field->errored = m_editor->ValidateField(windowType, field) == false;
			if (!field->errored) {
				OnUpdate(listIdx, field);
			}
		}
	}
	else if (ImGui::IsItemFocused())
	{
		if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
		{
			// Have to manually implement copying
			if (ImGui::IsKeyPressed(ImGuiKey_C, false)) {
				ImGui::SetClipboardText(field->buffer);
			}
			else if (ImGui::IsKeyPressed(ImGuiKey_X, false)) {
				moveFocusAway = true;
				field->QueueValue("");
				ImGui::SetClipboardText(field->buffer);
			}
			else if (ImGui::IsKeyPressed(ImGuiKey_V, false))
			{
				// ImGUI can sometimes fail this call and return NULL especially when storing images in the clipboard
				auto clipboardText = ImGui::GetClipboardText();
				if (clipboardText != nullptr) {
					try { field->QueueValue(std::string(EditorClipboard::Text(clipboardText))); moveFocusAway = true; m_operationError.clear(); }
                    catch (const std::exception& error) { m_operationError = error.what(); }
				}
			}
		}

		if ((field->flags & EditorInput_DataChangeable) && Keybinds::IsKeybindPressed("keybind_editor.change_data_type"))
		{
			if (!unsavedChanges) {
				// If no changes have been made, changing data type should not trigger the "unsaved changes" flag
				m_ignoreNextChange = true;
			}

			EditorUtils::ChangeFieldDataType(field);
			moveFocusAway = true;
		}
	}
	ImGui::PopID();
	ImGui::PopItemWidth();

	if (appliedBg) {
		ImGui::PopStyleColor();
	}
}

void EditorForm::CopyFormToClipboard() const
{
	std::string clipboardText = m_windowTypeName + "\n";
	for (auto& [fieldName, field] : m_fieldIdentifierMap)
	{
		clipboardText += field->nextValue ? *field->nextValue : field->buffer;
		clipboardText += "\n";
	}
	ImGui::SetClipboardText(clipboardText.c_str());
}

void EditorForm::PasteFormFromClipboard()
{
    try {
        PasteText(EditorClipboard::Text(ImGui::GetClipboardText()));
        m_operationError.clear();
    } catch (const std::exception& error) { m_operationError = error.what(); }
    catch (...) { m_operationError = "The clipboard operation failed."; }
}

std::vector<size_t> EditorForm::ClipboardWidths() const
{
    std::vector<size_t> widths;
    for (const auto& [name, field] : m_fieldIdentifierMap) widths.push_back(field && field->buffer ? field->bufsize : 0);
    return widths;
}

void EditorForm::PasteText(std::string_view text)
{
    const auto widths = ClipboardWidths();
    auto rows = EditorClipboard::Decode(text, m_windowTypeName, widths, ClipboardRowLimit());
    std::vector<std::optional<std::string>> values;
    for (auto& value : rows.front()) values.emplace_back(std::move(value));
    size_t column = 0;
    for (auto& [name, field] : m_fieldIdentifierMap) field->nextValue.swap(values[column++]);
    moveFocusAway = true;
    unsavedChanges = true;
}

bool EditorForm::CanPasteFormFromClipboard() const
{
    try {
        const auto widths = ClipboardWidths();
        EditorClipboard::Decode(EditorClipboard::Text(ImGui::GetClipboardText()), m_windowTypeName, widths, ClipboardRowLimit());
        return true;
    } catch (const std::exception&) { return false; }
}

void EditorForm::FlushPendingFields()
{
    for (auto& [name, field] : m_fieldIdentifierMap) {
        if (field->ConsumeNextValue()) {
            unsavedChanges = true;
            field->errored = !m_editor->ValidateField(windowType, field);
            if (!field->errored) OnUpdate(0, field);
        }
    }
}

bool EditorForm::IsFormValid() const
{
	for (auto& [category, fields] : m_fieldsCategoryMap) {
		for (auto& field : fields) {
			if (field->errored) {
				return false;
			}
		}
	}
	return true;
}

void EditorForm::ApplyWindowName()
{
	std::string windowName = _(std::format("edition.{}.window_name", m_windowTypeName).c_str());
	m_windowTitle = std::format("{} {} - {}###{}", windowName, structureId, m_windowTitleBase.c_str(), (uint64_t)this);
}

// -- Public methods -- //

void EditorForm::InitForm(std::string windowTitleBase, uint32_t t_id, EditorLogic* editor)
{
	structureId = t_id;
	m_editor = editor;
    if (!editor) throw std::invalid_argument("Cannot construct a form without an editor.");

	VectorSet<std::string> drawOrder;
	m_fieldIdentifierMap = editor->GetFormFields(windowType, t_id, drawOrder);
    EditorFormUtils::ValidateFields(m_fieldIdentifierMap, drawOrder);

	// Figure out the categories
	for (const std::string& fieldName : drawOrder) {
		EditorInput* field = m_fieldIdentifierMap.at(fieldName);
		std::string descriptionKey = field->fullName + ".description";
		field->hoverDescription = Localization::HasText(descriptionKey) ? _(descriptionKey.c_str()) : _(field->fullName.c_str());
		m_categories.insert(field->category);
	}

	// Builds the <category : fields> map
	for (uint8_t category : m_categories)
	{
		std::vector<EditorInput*> inputs;
		for (const auto& fieldName : drawOrder) {
			EditorInput* field = m_fieldIdentifierMap.at(fieldName);
			EditorFormUtils::SetFieldDisplayText(field, _(field->fullName.c_str()));
			if (field->category == category) {
				inputs.push_back(field);
			}
		}
		m_fieldsCategoryMap[category] = inputs;
		m_categoryStringIdentifiers[category] = std::format("edition.{}.category_{}", m_windowTypeName, category);
	}

	m_windowTitleBase = windowTitleBase;
	ApplyWindowName();
}

void EditorForm::RenderInternal()
{
	currentViewport = ImGui::GetWindowViewport();

	// Responsive form that tries to use big widths to draw up to 4 fields (+ 4 labels) per line
	const int columnCount = EditorFormUtils::GetColumnCount();
	m_labelWidthHalf = m_winInfo.size.x / columnCount / 2;
	for (uint8_t category : m_categories)
	{
		const int headerFlags = ImGuiTreeNodeFlags_Framed | (category & 1 ? 0 : ImGuiTreeNodeFlags_DefaultOpen);
		// todo: compute this std::format() once and not every frame
		if (category != 0 && !forceOpenCategories.empty() && forceOpenCategories.contains(_(m_categoryStringIdentifiers[category].c_str())))
			ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (category != 0 && !ImGui::CollapsingHeader(_(m_categoryStringIdentifiers[category].c_str()), headerFlags)) {
			// Only show titles for category > 0, and if tree is not open: no need to render anything
			continue;
		}

		else if (ImGui::BeginTable(m_windowTitle.c_str(), columnCount))
		{
			std::vector<EditorInput*>& inputs = m_fieldsCategoryMap[category];
			RenderInputs(0, inputs, category, columnCount);
			ImGui::EndTable();
		}

	}
}

void EditorForm::Render(bool parent_window_focused)
{
	if (setFocus) {
		ImGui::SetNextWindowScroll({ 0, 0 });
		ImGui::SetNextWindowFocus();
		setFocus = false;
	}

	if (nextDockId != -1) {
		ImGui::SetNextWindowDockID(nextDockId);
		nextDockId = -1;
	}

	// Setup style
	ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 0));

	ImGui::PushID(structureId);
    // Begin stores these in the window's dock-tab style. Restore them immediately
    // so record contents and other windows keep the shared palette.
    const int tabColours=AppTheme::PushTabAccent(FormTabAccent(windowType));
    const bool visible=ImGui::Begin(m_windowTitle.c_str(), &popen, unsavedChanges ? ImGuiWindowFlags_UnsavedDocument : 0);
    ImGui::PopStyleColor(tabColours);
	if (visible)
	{
		// Count focus inside child windows too (e.g. the T8 retail scrolling field area): otherwise clicking a field
		// or a header in a child reports the form as unfocused, which disables the right-click menu and shortcuts.
		windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		m_winInfo.pos = ImGui::GetWindowPos();
		m_winInfo.size = ImGui::GetWindowSize();

		if (m_requestedClosure) {
			RenderDiscardButtons();
		}
		else
		{
			// Render Apply button
			bool enabledBtn = unsavedChanges;
			const float buttonHeight = 25;// ImGui::GetFrameHeightWithSpacing();

			ImVec2 startPos = ImGui::GetCursorPos();

			if (enabledBtn) {
				ImGui::PushStyleColor(ImGuiCol_Button, FORM_SAVE_BTN);
			}
			ImGui::SetCursorPos(ImVec2(0, m_winInfo.size.y - buttonHeight + ImGui::GetScrollY()));
			if (ImGuiExtra::RenderButtonEnabled(_("edition.apply"), enabledBtn, ImVec2(ImGui::GetContentRegionAvail().x, buttonHeight))) {
				TryApply();
			}
			if (enabledBtn) {
				ImGui::PopStyleColor();
			}
		
			// Render form content
			ImGui::SetCursorPos(startPos);
			RenderInternal();
			if (!m_operationError.empty()) ImGuiExtra_TextboxError(m_operationError.c_str());

			// Render Apply button (again)
			// Yes, it's a bit off to render a button twice, but i didn't find any other reliable way to give priority to the button input
			if (ImGui::GetCursorPosY() + buttonHeight >= m_winInfo.size.y)
			{
				if (enabledBtn) {
					ImGui::PushStyleColor(ImGuiCol_Button, FORM_SAVE_BTN);
				}
				ImGui::Dummy(ImVec2(1, buttonHeight));
				// Render Apply() button at the bottom to create more scrollable space
				ImGui::SetCursorPos(ImVec2(0, m_winInfo.size.y - buttonHeight + ImGui::GetScrollY()));
				if (ImGuiExtra::RenderButtonEnabled(_("edition.apply"), enabledBtn, ImVec2(ImGui::GetContentRegionAvail().x, buttonHeight))) {
					TryApply();
				}
				if (enabledBtn) {
					ImGui::PopStyleColor();
				}
			}

			// Open popup on right click
			if (parent_window_focused && IsWindowRightClicked()) {
				ImGui::OpenPopup("ContextMenuPopup");
				m_canPaste = CanPasteFormFromClipboard();
			}

			// Context menu popup
			if (ImGui::BeginPopup("ContextMenuPopup"))
			{
                if (!m_operationError.empty()) ImGuiExtra_TextboxError(m_operationError.c_str());
				ImVec2 selectableSize(0, ImGui::GetFrameHeightWithSpacing());

				// Copy / Pasting
				if (ImGui::Selectable(_("edition.form_popup.copy_structure_clipboard"), false, 0, selectableSize))
				{
					ImGui::CloseCurrentPopup();
					CopyFormToClipboard();
				}

				{
					ImGuiExtra::DisableBlockIf __(!m_canPaste);
					if (ImGui::Selectable(_("edition.form_popup.paste_structure_clipboard"), false, 0, selectableSize) && CanPasteFormFromClipboard())
					{
						PasteFormFromClipboard();
						ImGui::CloseCurrentPopup();
					}
				}

				// Duplication
				ImGui::Separator();
				RenderItemContextMenuItems();
				{
					ImGuiExtra::DisableBlockIf __(uniqueType || !m_canDuplicate);
					if (ImGui::Selectable(_("edition.form_popup.duplicate_structure"), false, 0, selectableSize))
					{
                        TryDuplicateStructure();
					}
				}

				// Deletion
				ImGui::Separator();
				{
					ImGuiExtra::DisableBlockIf __(!m_isDeletable || uniqueType || structureId == 0);
					if (ImGui::Selectable(_("edition.form_popup.delete_structure"), false, 0, selectableSize)) {
						ImGui::CloseCurrentPopup();
						m_deletionPopupOpen = true;
					}
				}

				// Extras
				ImGui::Separator();
				RenderExtraContextMenuItems();
				ImGui::EndPopup();
			}

			if (m_deletionPopupOpen) {
				ImGui::OpenPopup("###StructureDeletionPopup");
			}

			if (ImGuiExtra::BeginFittedModal("StructureDeletionPopup", "Delete", &m_deletionPopupOpen, 28.0f))
			{
				ImGui::TextUnformatted(_("edition.form_popup.deletion_are_you_sure"));
                if (!m_operationError.empty()) ImGuiExtra_TextboxError(m_operationError.c_str());

				ImGui::PushStyleColor(ImGuiCol_Button, RED_BTN);
				if (ImGui::Button(_("yes"))) {
                    if (TryDeleteStructure()) ImGui::CloseCurrentPopup();
				}
				ImGui::PopStyleColor();

				ImGui::SameLine();
				if (ImGui::Button(_("no")) || ImGui::IsKeyDown(ImGuiKey_Escape)) {
					ImGui::CloseCurrentPopup();
					m_deletionPopupOpen = false;
				}
				ImGui::EndPopup();
			}
		}
	}
	ImGui::End();

	ImGui::PopID();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();

	if (!popen && unsavedChanges) {

		m_requestedClosure = true;
		popen = true;
	}

	PostRender();
}

void EditorForm::DeleteStructure()
{
    if (uniqueType || !m_isDeletable || structureId == 0) throw std::invalid_argument("This structure cannot be deleted.");
    uint64_t oldSize; const auto* oldBytes = m_editor->GetMoveset(oldSize);
    EditForm([&] {
        m_editor->DeleteStructures(windowType, structureId, 0);
        uint64_t size; const auto* bytes = m_editor->GetMoveset(size);
        if (size == oldSize && !memcmp(bytes, oldBytes, size))
            throw std::runtime_error("The editor did not delete this structure.");
        popen = false; unsavedChanges = false; justAppliedChanges = true;
        m_requestedClosure = false; m_deletionPopupOpen = false;
        OnDeletion();
        return true;
    });
}

void EditorForm::RenderFieldsInChild(const char* id, ImVec2 size)
{
	ImGui::BeginChild(id, size);
	const float width = m_winInfo.size.x;
	m_winInfo.size.x = ImGui::GetWindowWidth();
	EditorForm::RenderInternal();
	m_winInfo.size.x = width;
	ImGui::EndChild();
}

bool EditorForm::IsWindowRightClicked() const
{
	if (!ImGui::GetIO().MouseClicked[1] || m_ignoreRightClick) return false;

	auto cursorPos = ImGui::GetMousePos();
	auto& boundStart = m_winInfo.pos;

	DEBUG_LOG("Cursor: %f %f \n", cursorPos.x, cursorPos.y);
	DEBUG_LOG("Bound start: %f %f \n", boundStart.x, boundStart.y);

	if (cursorPos.x < boundStart.x) return false;
	if (cursorPos.y < (boundStart.y + ImGui::GetFrameHeightWithSpacing())) return false;

	auto boundEnd = m_winInfo.pos + m_winInfo.size;
	DEBUG_LOG("Bound end: %f %f \n", boundEnd.x, boundEnd.y);

	if (cursorPos.x >= boundEnd.x) return false;
	if (cursorPos.y >= boundEnd.y) return false;

	return true;
}

void EditorForm::OnUpdate(int listIdx, EditorInput* field)
{
    try {
        m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
    } catch (const std::exception& error) {
        m_editor->live_loadedMoveset = 0;
        m_operationError = error.what();
    }
}

EditorForm::~EditorForm()
{

	m_fieldIdentifierMap.clear();
	m_fieldsCategoryMap.clear();
	m_categories.clear();
	m_categoryStringIdentifiers.clear();
}
void EditorForm::TryApply()
{
    try {
        Apply();
        m_operationError.clear();
    } catch (const std::exception& error) {
        m_operationError = error.what();
    }
    catch (...) { m_operationError = "Applying the editor form failed."; }
}

struct EditorForm::SavedState final : TransactionState {
    EditorForm& form;
    bool committed = false;
    InputMap fields;
    std::map<int, std::vector<EditorInput*>> categories;
    decltype(EditorForm::m_operationError) m_operationError;
    decltype(EditorForm::m_windowTitle) m_windowTitle;
    decltype(EditorForm::m_categories) m_categories;
    decltype(EditorForm::m_consumedEvent) m_consumedEvent;
    decltype(EditorForm::m_editor) m_editor;
    decltype(EditorForm::m_windowTypeName) m_windowTypeName;
    decltype(EditorForm::m_baseWindow) m_baseWindow;
    decltype(EditorForm::m_requestedClosure) m_requestedClosure;
    decltype(EditorForm::m_categoryStringIdentifiers) m_categoryStringIdentifiers;
    decltype(EditorForm::m_windowTitleBase) m_windowTitleBase;
    decltype(EditorForm::m_winInfo) m_winInfo;
    decltype(EditorForm::m_labelWidthHalf) m_labelWidthHalf;
    decltype(EditorForm::m_ignoreNextChange) m_ignoreNextChange;
    decltype(EditorForm::m_deletionPopupOpen) m_deletionPopupOpen;
    decltype(EditorForm::m_isDeletable) m_isDeletable;
    decltype(EditorForm::m_canDuplicate) m_canDuplicate;
    decltype(EditorForm::m_canPaste) m_canPaste;
    decltype(EditorForm::popen) popen;
    decltype(EditorForm::unsavedChanges) unsavedChanges;
    decltype(EditorForm::justAppliedChanges) justAppliedChanges;
    decltype(EditorForm::setFocus) setFocus;
    decltype(EditorForm::structureId) structureId;
    decltype(EditorForm::windowType) windowType;
    decltype(EditorForm::nextDockId) nextDockId;
    decltype(EditorForm::currentViewport) currentViewport;
    decltype(EditorForm::uniqueType) uniqueType;
    decltype(EditorForm::moveFocusAway) moveFocusAway;
    decltype(EditorForm::windowFocused) windowFocused;
    SavedState(EditorForm& target, size_t& budget) : form(target),
        m_operationError(target.m_operationError),
        m_windowTitle(target.m_windowTitle),
        m_categories(target.m_categories),
        m_consumedEvent(target.m_consumedEvent),
        m_editor(target.m_editor),
        m_windowTypeName(target.m_windowTypeName),
        m_baseWindow(target.m_baseWindow),
        m_requestedClosure(target.m_requestedClosure),
        m_categoryStringIdentifiers(target.m_categoryStringIdentifiers),
        m_windowTitleBase(target.m_windowTitleBase),
        m_winInfo(target.m_winInfo),
        m_labelWidthHalf(target.m_labelWidthHalf),
        m_ignoreNextChange(target.m_ignoreNextChange),
        m_deletionPopupOpen(target.m_deletionPopupOpen),
        m_isDeletable(target.m_isDeletable),
        m_canDuplicate(target.m_canDuplicate),
        m_canPaste(target.m_canPaste),
        popen(target.popen),
        unsavedChanges(target.unsavedChanges),
        justAppliedChanges(target.justAppliedChanges),
        setFocus(target.setFocus),
        structureId(target.structureId),
        windowType(target.windowType),
        nextDockId(target.nextDockId),
        currentViewport(target.currentViewport),
        uniqueType(target.uniqueType),
        moveFocusAway(target.moveFocusAway),
        windowFocused(target.windowFocused) {
        auto copy = EditorFormUtils::CloneFields(form.m_fieldIdentifierMap, budget);
        auto categoryCopy = EditorFormUtils::CloneCategories(form.m_fieldIdentifierMap, copy, form.m_fieldsCategoryMap);
        fields.swap(form.m_fieldIdentifierMap); form.m_fieldIdentifierMap.swap(copy);
        categories.swap(form.m_fieldsCategoryMap); form.m_fieldsCategoryMap.swap(categoryCopy);
    }
    ~SavedState() override {
        if (committed) return;
        fields.swap(form.m_fieldIdentifierMap);
        categories.swap(form.m_fieldsCategoryMap);
        std::swap(m_operationError, form.m_operationError);
        std::swap(m_windowTitle, form.m_windowTitle);
        std::swap(m_categories, form.m_categories);
        std::swap(m_consumedEvent, form.m_consumedEvent);
        std::swap(m_editor, form.m_editor);
        std::swap(m_windowTypeName, form.m_windowTypeName);
        std::swap(m_baseWindow, form.m_baseWindow);
        std::swap(m_requestedClosure, form.m_requestedClosure);
        std::swap(m_categoryStringIdentifiers, form.m_categoryStringIdentifiers);
        std::swap(m_windowTitleBase, form.m_windowTitleBase);
        std::swap(m_winInfo, form.m_winInfo);
        std::swap(m_labelWidthHalf, form.m_labelWidthHalf);
        std::swap(m_ignoreNextChange, form.m_ignoreNextChange);
        std::swap(m_deletionPopupOpen, form.m_deletionPopupOpen);
        std::swap(m_isDeletable, form.m_isDeletable);
        std::swap(m_canDuplicate, form.m_canDuplicate);
        std::swap(m_canPaste, form.m_canPaste);
        std::swap(popen, form.popen);
        std::swap(unsavedChanges, form.unsavedChanges);
        std::swap(justAppliedChanges, form.justAppliedChanges);
        std::swap(setFocus, form.setFocus);
        std::swap(structureId, form.structureId);
        std::swap(windowType, form.windowType);
        std::swap(nextDockId, form.nextDockId);
        std::swap(currentViewport, form.currentViewport);
        std::swap(uniqueType, form.uniqueType);
        std::swap(moveFocusAway, form.moveFocusAway);
        std::swap(windowFocused, form.windowFocused);
    }
    void Commit() noexcept override { committed = true; }
};
std::unique_ptr<EditorForm::TransactionState> EditorForm::BeginTransaction(size_t& budget) {
    return std::make_unique<SavedState>(*this, budget);
}

std::unique_ptr<EditorForm::TransactionState> EditorForm::BeginEditTransaction() {
    if (m_baseWindow) return m_baseWindow->BeginFormTransaction(*this);
    size_t budget = 64 * 1024 * 1024;
    return BeginTransaction(budget);
}

void EditorForm::EditForm(const std::function<bool()>& action) {
    if (m_formEditActive) throw std::logic_error("A form callback cannot start another edit of the same form.");
    struct Active {
        bool& value;
        explicit Active(bool& flag) : value(flag) { value = true; }
        ~Active() { value = false; }
    } active(m_formEditActive);
    auto forms = BeginEditTransaction();
    {
        EditorLogic::Mutation editor(*m_editor, true);
        m_editor->live_loadedMoveset = 0;
        // False commits UI validation state only; editor changes are discarded.
        if (action()) editor.Commit();
    }
    m_editor->live_loadedMoveset = 0;
    forms->Commit();
}
uint32_t EditorForm::DuplicateStructure() {
    if (uniqueType || !m_canDuplicate) throw std::invalid_argument("This form cannot be duplicated.");
    uint32_t id = 0;
    uint64_t oldSize; const auto* oldBytes = m_editor->GetMoveset(oldSize);
    EditForm([&] {
        const int listSize = GetOriginalStructureListSize();
        if (listSize < 0) throw std::invalid_argument("Invalid original list size.");
        const auto amount = listSize ? static_cast<size_t>(listSize) : 1;
        id = m_editor->DuplicateStructure(windowType, structureId, amount);
        if (!(windowType & 0x80000000U) && (!id || id >= 65536 || amount > 65536 - id))
            throw std::out_of_range("Duplication returned an invalid destination range.");
        uint64_t size; const auto* bytes = m_editor->GetMoveset(size);
        if (size == oldSize && !memcmp(bytes, oldBytes, size))
            throw std::runtime_error("The editor did not duplicate this structure.");
        justAppliedChanges = true;
        PrepareDuplication(id, listSize);
        OpenDuplicatedForm(id, listSize);
        OnDuplication(id, listSize);
        return true;
    });
    return id;
}
void EditorForm::OpenDuplicatedForm(uint32_t id, int listSize) {
    if (!m_baseWindow || !m_baseWindow->OpenFormWindow(windowType, static_cast<uint16_t>(id), listSize))
        throw std::runtime_error("Could not open the duplicated structure.");
}
void EditorForm::OnDeletion() {
    if (m_baseWindow) m_baseWindow->IssueFieldUpdate(windowType, -1, structureId, structureId + 1);
}
bool EditorForm::TryDuplicateStructure() {
    try { DuplicateStructure(); m_operationError.clear(); return true; }
    catch (const std::exception& error) { m_operationError = error.what(); }
    catch (...) { m_operationError = "Duplicating the structure failed."; }
    return false;
}
bool EditorForm::TryDeleteStructure() {
    try {
        DeleteStructure(); justAppliedChanges = true; m_deletionPopupOpen = false;
        m_operationError.clear(); return true;
    }
    catch (const std::exception& error) { m_operationError = error.what(); }
    catch (...) { m_operationError = "Deleting the structure failed."; }
    return false;
}
