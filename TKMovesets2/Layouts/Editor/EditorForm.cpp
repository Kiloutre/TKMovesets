#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorForm.hpp"
#include "Localization.hpp"

// -- Helpers -- //

namespace EditorFormUtils
{
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

	std::string GetWindowTypeName(EditorWindowType_ type)
	{
		switch (type)
		{
		case EditorWindowType_Move:
			return "move";
		case EditorWindowType_Voiceclip:
			return "voiceclip";
		case EditorWindowType_Extraproperty:
			return "extraproperty";
		case EditorWindowType_Cancel:
			return "cancel";
		case EditorWindowType_Requirement:
			return "requirement";
			//todo
		}
		return "UNKNOWN";
	}
}


// -- Private methods -- //

void EditorForm::Apply()
{
	if (!IsFormValid()) {
		return;
	}

	m_editor->SaveItem(windowType, id, m_fieldIdentifierMap);
	unsavedChanges = false;
	justAppliedChanges = true;
}

void EditorForm::RenderInputs(std::vector<EditorInput*>& inputs, int category, int columnCount)
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
		RenderLabel(field);

		// Render input field
		if (columnCount == 1) {
			ImGui::TableNextRow();
		}
		ImGui::TableNextColumn();
		RenderInput(field);
	}
}

void EditorForm::RenderLabel(EditorInput* field)
{
	if (field->flags & EditorInput_Clickable) {
		if (ImGui::Selectable(_(field->field_fullname.c_str()), !field->errored)) {
			m_consumedEvent = false;
			m_event.eventName = field->field_fullname;
			m_event.buffer = field->buffer;
		}
	} else {
		ImGui::TextUnformatted(_(field->field_fullname.c_str()));
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
			// todo: maybe use this for a full-on description
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(_(field->field_fullname.c_str()));
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}
}

void EditorForm::RenderInput(EditorInput* field)
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
	}
	/*
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
			field->errored = m_editor->ValidateField(windowType, field->name, field) == false;
		}
	}
	*/
	ImGui::PopID();
	ImGui::PopItemWidth();

	if (erroredBg) {
		ImGui::PopStyleColor();
	}
}

bool EditorForm::IsFormValid()
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

// -- Public methods -- //

void EditorForm::InitForm(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	id = t_id;
	m_editor = editor;

	m_identifierPrefix = "edition." + EditorFormUtils::GetWindowTypeName(windowType) + "_field";

	VectorSet<std::string> drawOrder;
	m_fieldIdentifierMap = editor->GetFormFields(windowType, t_id, drawOrder);

	// Tries to find a name to show in the window title
	// Also figure out the categories
	char name[32] = "";
	for (std::string fieldName : drawOrder) {
		EditorInput* field = m_fieldIdentifierMap[fieldName];
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
			EditorInput* field = m_fieldIdentifierMap[fieldName];
			if (field->category == category) {
				inputs.push_back(field);
			}
		}
		m_fieldsCategoryMap[category] = inputs;
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

void EditorForm::Render()
{
	if (setFocus) {
		ImGui::SetNextWindowFocus();
		setFocus = false;
	}

	if (ImGui::Begin(m_windowTitle.c_str(), &popen, unsavedChanges ? ImGuiWindowFlags_UnsavedDocument : 0))
	{
		// Responsive form that tries to use big widths to draw up to 4 fields (+ 4 labels) per line
		const int columnCount = EditorFormUtils::GetColumnCount();
		for (uint8_t category : m_categories)
		{
			const int headerFlags = ImGuiTreeNodeFlags_Framed | (category & 1 ? 0 : ImGuiTreeNodeFlags_DefaultOpen);
			if (category != 0 && !ImGui::CollapsingHeader(_(std::format("{}.category_{}", m_identifierPrefix, category).c_str()), headerFlags)) {
				// Only show titles for category > 0, and if tree is not open: no need to render anything
				continue;
			}

			if (ImGui::BeginTable(m_windowTitle.c_str(), columnCount))
			{
				std::vector<EditorInput*>& inputs = m_fieldsCategoryMap[category];
				RenderInputs(inputs, category, columnCount);
				ImGui::EndTable();
			}
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
	ImGui::End();
	
	if (!popen && unsavedChanges) {
		// Ordered to close, but changes remain
		// todo: show popup, force popen = true
	}
}

ClickableFieldEvent* EditorForm::GetFormClickEvent()
{
	if (!m_consumedEvent)
	{
		m_consumedEvent = true;
		return &m_event;
	}
	return nullptr;
}