#include <format>

#include "imgui_extras.hpp"
#include "EditorGroupedCancels.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorGroupedCancels::EditorGroupedCancels(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_GroupedCancel;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

// -- Private methods -- //

void EditorGroupedCancels::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	auto& name = field->name;
	auto& item = m_items[listIdx];

	if (name == "move_id") {
		m_baseWindow->OpenFormWindow(EditorWindowType_Move, m_baseWindow->ValidateMoveId(field->buffer));
	}
	else if (name == "extradata_addr") {
		int id = atoi(field->buffer);
		m_baseWindow->OpenFormWindow(EditorWindowType_CancelExtradata, id);
	}
	else if (name == "requirements_addr") {
		int id = atoi(field->buffer);
		m_baseWindow->OpenFormWindow(EditorWindowType_Requirement, id);
	}
	else if (name == "command") {
		// Command is only clickable
		// Command is only clickable if we detected that it was an input sequence reference in OnUpdate()
		int inputSequenceId = m_editor->GetCommandInputSequenceID(item->identifierMap["command"]->buffer);
		m_baseWindow->OpenFormWindow(EditorWindowType_InputSequence, inputSequenceId);
	}
}

void EditorGroupedCancels::BuildItemDetails(int listIdx)
{
	std::string label;

	auto& item = m_items[listIdx];

	EditorInput* commandField = item->identifierMap["command"];
	EditorInput* moveIdField = item->identifierMap["move_id"];

	if (m_editor->IsCommandInputSequence(commandField->buffer))
	{
		commandField->flags |= EditorInput_Clickable;
		int inputSequenceId = m_editor->GetCommandInputSequenceID(commandField->buffer);

		item->color = MOVEID_INPUT_SEQUENCE;
		EditorFormUtils::SetFieldDisplayText(commandField, _("edition.cancel.sequence_id"));

		int move_id = atoi(moveIdField->buffer);
		int validated_move_id = m_baseWindow->ValidateMoveId(moveIdField->buffer);


		std::string inputs;

		if (inputSequenceId >= (int)m_editor->GetStructureCount(EditorWindowType_InputSequence))
		{
			commandField->errored = true;
			inputs = _("edition.cancel.invalid_sequence_id");
		}
		else
		{
			int inputAmount = 0;
			m_editor->GetInputSequenceString(inputSequenceId, inputs, inputAmount);

			if (inputAmount > 0) {
				inputs = std::format("{} {}", inputAmount, _("edition.input.inputs"));
			}
		}

		if (m_baseWindow->ValidateMoveId(moveIdField->buffer) == -1) {
			label = std::format("{} / {} / {}: {}, [ {} ]", move_id, _("edition.form_list.invalid"), _("edition.input_sequence.window_name"), inputSequenceId, inputs);
		}
		else {
			const char* moveName = m_baseWindow->movelist->at(validated_move_id)->name.c_str();
			label = std::format("{} / {} / {}: {}, [ {} ]", move_id, moveName, _("edition.input_sequence.window_name"), inputSequenceId, inputs);
		}
	}
	else {
		EditorFormUtils::SetFieldDisplayText(commandField, _("edition.cancel.command"));
		if (commandField->flags & EditorInput_Clickable) {
			commandField->flags -= EditorInput_Clickable;
		}


		int move_id = atoi(moveIdField->buffer);
		int validated_move_id = m_baseWindow->ValidateMoveId(moveIdField->buffer);

		item->color = 0;
		EditorFormUtils::SetFieldDisplayText(moveIdField, _("edition.cancel.move_id"));
		std::string commandStr = m_editor->GetCommandStr(commandField->buffer);

		if (validated_move_id == -1) {
			label = std::format("{} ({}) / [ {} ]", _("edition.form_list.invalid"), move_id, commandStr);
		}
		else
		{
			const char* moveName = m_baseWindow->movelist->at(validated_move_id)->name.c_str();
			label = std::format("{} / {} / [ {} ]", move_id, moveName, commandStr);
		}
	}

	item->itemLabel = label;
}


void EditorGroupedCancels::RequestFieldUpdate(EditorWindowType_ winType, int valueChange, int listStart, int listEnd)
{
	switch (winType)
	{
	case EditorWindowType_GroupedCancel:
		// If a struct was created before this one, we must shfit our own ID
		if (MUST_SHIFT_ID(structureId, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
			structureId += valueChange;
			ApplyWindowName();
		}
		break;
	case EditorWindowType_Requirement:
	{
		int listIdx = 0;
		for (auto& item : m_items)
		{
			EditorInput* field = item->identifierMap["requirements_addr"];

			if (field->errored) {
				continue;
			}

			int value = atoi(field->buffer);
			if (MUST_SHIFT_ID(value, valueChange, listStart, listEnd)) {
				// Same shifting logic as in ListCreations
				// Might be a good idea to macro it
				sprintf_s(field->buffer, field->bufsize, "%d", value + valueChange);
				BuildItemDetails(listIdx);
			}

			++listIdx;
		}
	}
	break;
	}
}

void EditorGroupedCancels::RenderExtraContextMenuItems()
{
	ImVec2 selectableSize(0, ImGui::GetFrameHeightWithSpacing());

	ImGuiExtra::DisableBlockIf __(true);
	if (ImGui::Selectable(_("edition.form_popup.edition.form_popup.what_moves_use_this_cancel_list "), false, 0, selectableSize))
	{
		//todo
	}
}