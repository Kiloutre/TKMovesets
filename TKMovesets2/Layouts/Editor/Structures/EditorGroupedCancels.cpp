#include <ImGui.h>
#include <format>
#include <string>

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
	int id = atoi(field->buffer);
	std::string& name = field->name;
	auto& item = m_items[listIdx];

	if (name == "move_id") {
		uint64_t command = (uint64_t)strtoll(item->identifierMaps["command"]->buffer, nullptr, 16);
		if (command == m_editor->constants[EditorConstants_GroupedCancelCommand]) {
			m_baseWindow->OpenFormWindow(EditorWindowType_GroupedCancel, id);
		}
		else {
			m_baseWindow->OpenFormWindow(EditorWindowType_Move, m_baseWindow->ValidateMoveId(field->buffer));
		}
	}
	else if (name == "extradata_addr") {
		m_baseWindow->OpenFormWindow(EditorWindowType_CancelExtradata, id);
	}
	else if (name == "requirements_addr") {
		m_baseWindow->OpenFormWindow(EditorWindowType_Requirement, id);
	}
	else if (name == "command") {
		// Command is only clickable
		uint64_t command = (uint64_t)strtoll(item->identifierMaps["command"]->buffer, nullptr, 16);
		int inputSequenceId = (command & 0xFFFFFFFF) - m_editor->constants[EditorConstants_InputSequenceCommandStart];
		m_baseWindow->OpenFormWindow(EditorWindowType_InputSequence, inputSequenceId);
	}
}

void EditorGroupedCancels::BuildItemDetails(int listIdx)
{
	std::string label;

	auto& item = m_items[listIdx];

	EditorInput* commandField = item->identifierMaps["command"];
	EditorInput* moveIdField = item->identifierMaps["move_id"];
	uint64_t command = (uint64_t)strtoll(commandField->buffer, nullptr, 16);

	if (m_editor->IsCommandInputSequence(command)) {
		commandField->flags |= EditorInput_Clickable;
		int inputSequenceId = (command & 0xFFFFFFFF) - m_editor->constants[EditorConstants_InputSequenceCommandStart];

		item->color = MOVEID_INPUT_SEQUENCE;
		commandField->displayName = "edition.cancel.sequence_id";

		int move_id = atoi(moveIdField->buffer);
		int validated_move_id = m_baseWindow->ValidateMoveId(moveIdField->buffer);

		std::string inputs;
		int inputAmount = 0;
		m_editor->GetInputSequenceString(inputSequenceId, inputs, inputAmount);

		if (inputAmount > 0) {
			inputs = std::format("{} {}", inputAmount, _("edition.inputs.amount"));
		}

		if (m_baseWindow->ValidateMoveId(moveIdField->buffer) == -1) {
			label = std::format("{} / {} / {}: {}, [ {} ]", move_id, _("edition.form_list.invalid"), _("edition.input_sequence.window_name"), inputSequenceId, inputs);
		}
		else {
			const char* moveName = m_baseWindow->movelist[validated_move_id]->name.c_str();
			label = std::format("{} / {} / {}: {}, [ {} ]", move_id, moveName, _("edition.input_sequence.window_name"), inputSequenceId, inputs);
		}
	}
	else {
		commandField->displayName = "edition.cancel.command";
		if (commandField->flags & EditorInput_Clickable) {
			commandField->flags -= EditorInput_Clickable;
		}


		int move_id = atoi(moveIdField->buffer);
		int validated_move_id = m_baseWindow->ValidateMoveId(moveIdField->buffer);

		item->color = 0;
		moveIdField->displayName = "edition.cancel.move_id";
		std::string commandStr = m_editor->GetCommandStr(commandField->buffer);

		if (validated_move_id == -1) {
			label = std::format("{} ({}) / [ {} ]", _("edition.form_list.invalid"), move_id, commandStr);
		}
		else
		{
			const char* moveName = m_baseWindow->movelist[validated_move_id]->name.c_str();
			label = std::format("{} / {} / [ {} ]", move_id, moveName, commandStr);
		}
	}

	item->itemLabel = label;
}

void EditorGroupedCancels::OnResize(int sizeChange, int oldSize)
{
	m_baseWindow->IssueFieldUpdate("grouped_cancel_id", sizeChange, id, id + oldSize);
}

void EditorGroupedCancels::RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd)
{
	if (fieldName == "input_sequence_id")
	{
		int listIdx = 0;
		for (auto& item : m_items)
		{
			EditorInput* commandField = item->identifierMaps["command"];
			uint64_t command = (uint64_t)strtoll(commandField->buffer, nullptr, 16);

			if (!m_editor->IsCommandInputSequence(command)) {
				continue;
			}

			const int inputSequenceStart = m_editor->constants[EditorConstants_InputSequenceCommandStart];
			int inputSequenceId = (command & 0xFFFFFFFF) - inputSequenceStart;

			if (MUST_SHIFT_ID(inputSequenceId, valueChange, listStart, listEnd)) {
				// Same shifting logic as in ListCreations
				// Might be a good idea to macro it
				sprintf(commandField->buffer, "%d", inputSequenceStart + inputSequenceId + valueChange);
				BuildItemDetails(listIdx);
			}

			++listIdx;
		}
	}
	else if (fieldName == "requirements_addr")
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
				sprintf(field->buffer, "%d", value + valueChange);
				BuildItemDetails(listIdx);
			}

			++listIdx;
		}
	}
}