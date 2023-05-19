#include <format>

#include "TEditorStructures.hpp"
#include "Localization.hpp"
#include "EditorVisuals_t7.hpp"
#include "imgui_extras.hpp"

// -- Private methods -- //

void EditorCancels::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	auto editor = Editor<EditorT7>();

	int referenceId = atoi(field->buffer);
	auto& name = field->name;
	auto& item = m_items[listIdx];

	if (name == "move_id") {
		if (editor->IsCommandGroupedCancelReference(item->identifierMap["command"]->buffer)) {
			m_baseWindow->OpenFormWindow(TEditorWindowType_GroupedCancel, referenceId);
		}
		else {
			auto moveId = BaseWindow<EditorVisuals_T7>()->ValidateMoveId(field->buffer);
			m_baseWindow->OpenFormWindow(TEditorWindowType_Move, moveId);
		}
	}
	else if (name == "extradata_addr") {
		m_baseWindow->OpenFormWindow(TEditorWindowType_CancelExtradata, referenceId);
	}
	else if (name == "requirements_addr") {
		m_baseWindow->OpenFormWindow(TEditorWindowType_Requirement, referenceId);
	}
	else if (name == "command") {
		// Command is only clickable if we detected that it was an input sequence reference in OnUpdate()
		int inputSequenceId = editor->GetCommandInputSequenceID(item->identifierMap["command"]->buffer);
		m_baseWindow->OpenFormWindow(TEditorWindowType_InputSequence, inputSequenceId);
	}
}

void EditorCancels::OnUpdate(int listIdx, EditorInput* field)
{
	auto& name = field->name;
	auto& item = m_items[listIdx];
	auto baseWindow = BaseWindow<EditorVisuals_T7>();
	auto editor = Editor<EditorT7>();

	if (name == "command" || name == "move_id") {
		// More complex validation than what is shown in the editors.cpp (cross-field validation)
		EditorInput* commandField = item->identifierMap["command"];
		EditorInput* moveIdField = item->identifierMap["move_id"];

		if (editor->IsCommandGroupedCancelReference(commandField->buffer))
		{
			int groupId = atoi(moveIdField->buffer);
			moveIdField->errored = groupId >= (int)editor->GetStructureCount(TEditorWindowType_GroupedCancel);
		}
		else
		{
			auto moveId = baseWindow->ValidateMoveId(moveIdField->buffer);
			moveIdField->errored = moveId == -1;
		}
	}

	BuildItemDetails(listIdx);

	m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
}

void EditorCancels::BuildItemDetails(int listIdx)
{
	auto baseWindow = BaseWindow<EditorVisuals_T7>();
	auto editor = Editor<EditorT7>();

	std::string label;
	auto& item = m_items[listIdx];

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

		auto m = baseWindow->ValidateMoveId(moveIdField->buffer);
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
		if (editor->IsCommandGroupedCancelReference(commandField->buffer))
		{
			EditorFormUtils::SetFieldDisplayText(moveIdField, _("edition.cancel.group_id"));
			label = std::format("{}: {}", _("edition.grouped_cancel.window_name"), move_id);
			item->color = MOVEID_GROUP_CANCEL;
		}
		else
		{
			item->color = 0;
			EditorFormUtils::SetFieldDisplayText(moveIdField, _("edition.cancel.move_id"));
			int validated_move_id = baseWindow->ValidateMoveId(moveIdField->buffer);
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
	}

	item->itemLabel = label;
}

void EditorCancels::RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd)
{
	auto editor = Editor<EditorT7>();

	switch (winType)
	{
	case TEditorWindowType_Cancel:
		// If a struct was created before this one, we must shfit our own ID
		if (MUST_SHIFT_ID(structureId, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
			structureId += valueChange;
			ApplyWindowName();
		}
		break;
	case TEditorWindowType_GroupedCancel:
		{
			int listIdx = 0;
			for (auto& item : m_items)
			{
				EditorInput* commandField = item->identifierMap["command"];
				EditorInput* moveIdField = item->identifierMap["move_id"];

				if (moveIdField->errored || commandField->errored) {
					continue;
				}

				if (editor->IsCommandGroupedCancelReference(commandField->buffer))
				{
					int group_id = atoi(moveIdField->buffer);
					if (MUST_SHIFT_ID(group_id, valueChange, listStart, listEnd)) {
						// Same shifting logic as in ListCreations
						sprintf_s(moveIdField->buffer, moveIdField->bufsize, "%d", group_id + valueChange);
						BuildItemDetails(listIdx);
					}
				}

				++listIdx;
			}

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

void EditorCancels::RenderExtraContextMenuItems()
{
	ImVec2 selectableSize(0, ImGui::GetFrameHeightWithSpacing());

	ImGuiExtra::DisableBlockIf __(true);
	if (ImGui::Selectable(_("edition.form_popup.edition.form_popup.what_moves_use_this_cancel_list "), false, 0, selectableSize))
	{
		//todo
	}
}