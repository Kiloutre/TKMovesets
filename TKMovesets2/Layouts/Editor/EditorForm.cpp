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
		case EditorWindowType_CancelExtradata:
			return "cancel_extra";
		case EditorWindowType_Requirement:
			return "requirement";
		case EditorWindowType_GroupedCancel:
			return "grouped_cancel";
		case EditorWindowType_HitCondition:
			return "hit_condition";
		case EditorWindowType_Reactions:
			return "reactions";
		case EditorWindowType_Pushback:
			return "pushback";
		case EditorWindowType_PushbackExtradata:
			return "pushback_extradata";
		case EditorWindowType_MoveBeginProperty:
			return "start_other_extraproperty";
		case EditorWindowType_MoveEndProperty:
			return "end_other_extraproperty";
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
	m_requestedClosure = false;

	OnApply();
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
	const char* fieldLabel = _(field->field_fullname.c_str());
	if (field->flags & EditorInput_Clickable && !field->errored) {
		if (ImGui::Selectable(fieldLabel, true)) {
			OnFieldLabelClick(field);
		}
	} else {
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
		if (!field->errored) {
			OnUpdate(0, field);
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

	m_identifierPrefix = "edition." + EditorFormUtils::GetWindowTypeName(windowType);

	VectorSet<std::string> drawOrder;
	m_fieldIdentifierMap = editor->GetFormFields(windowType, t_id, drawOrder);

	// Tries to find a name to show in the window title
	// Also figure out the categories
	char name[32] = "";
	for (const std::string& fieldName : drawOrder) {
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

void EditorForm::Render()
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
			for (uint8_t category : m_categories)
			{
				const int headerFlags = ImGuiTreeNodeFlags_Framed | (category & 1 ? 0 : ImGuiTreeNodeFlags_DefaultOpen);
				// todo: compute this std::format() once and not every frame
				if (category != 0 && !ImGui::CollapsingHeader(_(m_categoryStringIdentifiers[category].c_str()), headerFlags)) {
					// Only show titles for category > 0, and if tree is not open: no need to render anything
					continue;
				}

				else if (ImGui::BeginTable(m_windowTitle.c_str(), columnCount))
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
	}

	ImGui::End();

	if (!popen && unsavedChanges) {

		m_requestedClosure = true;
		popen = true;
	}
}