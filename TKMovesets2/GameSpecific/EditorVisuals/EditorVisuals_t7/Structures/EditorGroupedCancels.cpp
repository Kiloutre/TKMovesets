#include <format>

#include "TEditorStructures.hpp"
#include "Localization.hpp"
#include "EditorVisuals_t7.hpp"
#include "imgui_extras.hpp"

// -- Private methods -- //

void EditorGroupedCancels::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	auto editor = Editor<EditorT7>();

	auto& name = field->name;
	auto& item = m_items[listIdx];
	auto baseWindow = BaseWindow<EditorVisuals_T7>();

	if (name == "move_id") {
		m_baseWindow->OpenFormWindow(TEditorWindowType_Move, baseWindow->ValidateMoveId(field->buffer));
	}
	else if (name == "extradata_addr") {
		int id = atoi(field->buffer);
		m_baseWindow->OpenFormWindow(TEditorWindowType_CancelExtradata, id);
	}
	else if (name == "requirements_addr") {
		int id = atoi(field->buffer);
		m_baseWindow->OpenFormWindow(TEditorWindowType_Requirement, id);
	}
	else if (name == "command") {
		// Command is only clickable
		// Command is only clickable if we detected that it was an input sequence reference in OnUpdate()
		int inputSequenceId = editor->GetCommandInputSequenceID(item->identifierMap["command"]->buffer);
		m_baseWindow->OpenFormWindow(TEditorWindowType_InputSequence, inputSequenceId);
	}
}

void EditorGroupedCancels::BuildItemDetails(int listIdx)
{
	auto editor = Editor<EditorT7>();

	std::string label;

	auto& item = m_items[listIdx];
	auto baseWindow = BaseWindow<EditorVisuals_T7>();

	EditorInput* commandField = item->identifierMap["command"];
	EditorInput* moveIdField = item->identifierMap["move_id"];

	if (editor->IsCommandInputSequence(commandField->buffer))
	{
		commandField->flags |= EditorInput_Clickable;
		int inputSequenceId = editor->GetCommandInputSequenceID(commandField->buffer);

		item->color = MOVEID_INPUT_SEQUENCE;
		EditorFormUtils::SetFieldDisplayText(commandField, _("edition.cancel.sequence_id"));

		int move_id = atoi(moveIdField->buffer);
		int validated_move_id = baseWindow->ValidateMoveId(moveIdField->buffer);


		std::string inputs;

		if (inputSequenceId >= (int)editor->GetStructureCount(TEditorWindowType_InputSequence))
		{
			commandField->errored = true;
			inputs = _("edition.cancel.invalid_sequence_id");
		}
		else
		{
			int inputAmount = 0;
			editor->GetInputSequenceString(inputSequenceId, inputs, inputAmount);

			if (inputAmount > 0) {
				inputs = std::format("{} {}", inputAmount, _("edition.input.inputs"));
			}
		}


		if (validated_move_id == -1) {
			label = std::format("{} / {} / {}: {}, [ {} ]", move_id, _("edition.form_list.invalid"), _("edition.input_sequence.window_name"), inputSequenceId, inputs);
		}
		else {
			const char* moveName = baseWindow->movelist->at(validated_move_id)->name.c_str();
			label = std::format("{} / {} / {}: {}, [ {} ]", move_id, moveName, _("edition.input_sequence.window_name"), inputSequenceId, inputs);
		}
        
	}
	else {
		EditorFormUtils::SetFieldDisplayText(commandField, _("edition.cancel.command"));
		if (commandField->flags & EditorInput_Clickable) {
			commandField->flags -= EditorInput_Clickable;
		}


		int move_id = atoi(moveIdField->buffer);
		int validated_move_id = baseWindow->ValidateMoveId(moveIdField->buffer);

		item->color = 0;
		EditorFormUtils::SetFieldDisplayText(moveIdField, _("edition.cancel.move_id"));
		std::string commandStr = editor->GetCommandStr(commandField->buffer);

		if (validated_move_id == -1) {
			label = std::format("{} ({}) / [ {} ]", _("edition.form_list.invalid"), move_id, commandStr);
		}
		else
		{
			const char* moveName = baseWindow->movelist->at(validated_move_id)->name.c_str();
			label = std::format("{} / {} / [ {} ]", move_id, moveName, commandStr);
		}
	}

	item->itemLabel = label;
}


void EditorGroupedCancels::RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd)
{
	switch (winType)
	{
	case TEditorWindowType_GroupedCancel:
		// If a struct was created before this one, we must shfit our own ID
		if (MUST_SHIFT_ID(structureId, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
			structureId += valueChange;
			ApplyWindowName();
		}
		break;
	case TEditorWindowType_Requirement:
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