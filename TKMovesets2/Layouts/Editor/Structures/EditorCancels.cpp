#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorCancels.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorCancels::EditorCancels(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_Cancel;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);

	m_usesNewerApplyFunction = true;
}

// -- Private methods -- //

void EditorCancels::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	int referenceId = atoi(field->buffer);
	std::string& name = field->name;
	auto& item = m_items[listIdx];

	if (name == "move_id") {
		if (m_editor->IsCommandGroupedCancelReference(item->identifierMaps["command"]->buffer)) {
			m_baseWindow->OpenFormWindow(EditorWindowType_GroupedCancel, referenceId);
		}
		else {
			m_baseWindow->OpenFormWindow(EditorWindowType_Move, m_baseWindow->ValidateMoveId(field->buffer));
		}
	}
	else if (name == "extradata_addr") {
		m_baseWindow->OpenFormWindow(EditorWindowType_CancelExtradata, referenceId);
	}
	else if (name == "requirements_addr") {
		m_baseWindow->OpenFormWindow(EditorWindowType_Requirement, referenceId);
	}
	else if (name == "command") {
		// Command is only clickable if we detected that it was an input sequence reference in OnUpdate()
		int inputSequenceId = m_editor->GetCommandInputSequenceID(item->identifierMaps["command"]->buffer);
		m_baseWindow->OpenFormWindow(EditorWindowType_InputSequence, inputSequenceId);
	}
}

void EditorCancels::OnUpdate(int listIdx, EditorInput* field)
{
	std::string& name = field->name;
	auto& item = m_items[listIdx];

	if (name == "command" || name == "move_id") {
		// More complex validation than what is shown in the editors.cpp (cross-field validation)
		EditorInput* commandField = item->identifierMaps["command"];
		EditorInput* moveIdField = item->identifierMaps["move_id"];

		if (m_editor->IsCommandGroupedCancelReference(commandField->buffer))
		{
			int groupId = atoi(moveIdField->buffer);
			moveIdField->errored = groupId >= (int)m_editor->GetStructureCount(EditorWindowType_GroupedCancel);
		}
		else
		{
			int moveId = m_baseWindow->ValidateMoveId(moveIdField->buffer);
			moveIdField->errored = moveId == -1;
		}
	}

	BuildItemDetails(listIdx);

	m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
}

void EditorCancels::BuildItemDetails(int listIdx)
{
	std::string label;

	auto& item = m_items[listIdx];

	EditorInput* commandField = item->identifierMaps["command"];
	EditorInput* moveIdField = item->identifierMaps["move_id"];

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
		if (m_editor->IsCommandGroupedCancelReference(commandField->buffer))
		{
			EditorFormUtils::SetFieldDisplayText(moveIdField, _("edition.cancel.group_id"));
			label = std::format("{}: {}", _("edition.grouped_cancel.window_name"), move_id);
			item->color = MOVEID_GROUP_CANCEL;
		}
		else
		{
			item->color = 0;
			EditorFormUtils::SetFieldDisplayText(moveIdField, _("edition.cancel.move_id"));
			int validated_move_id = m_baseWindow->ValidateMoveId(moveIdField->buffer);
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
	}

	item->itemLabel = label;
}

void EditorCancels::RequestFieldUpdate(EditorWindowType_ winType, int valueChange, int listStart, int listEnd)
{
	switch (winType)
	{
	case EditorWindowType_Cancel:
		// If a struct was created before this one, we must shfit our own ID
		if (MUST_SHIFT_ID(structureId, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
			structureId += valueChange;
			ApplyWindowName();
		}
		break;
	case EditorWindowType_GroupedCancel:
		{
			int listIdx = 0;
			for (auto& item : m_items)
			{
				EditorInput* commandField = item->identifierMaps["command"];
				EditorInput* moveIdField = item->identifierMaps["move_id"];

				if (moveIdField->errored || commandField->errored) {
					continue;
				}

				if (m_editor->IsCommandGroupedCancelReference(commandField->buffer))
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
	case EditorWindowType_Requirement:
		{
			int listIdx = 0;
			for (auto& item : m_items)
			{
				EditorInput* field = item->identifierMaps["requirements_addr"];

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