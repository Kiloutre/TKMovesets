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
}

// -- Private methods -- //

void EditorCancels::OnFieldLabelClick(int listIdx, EditorInput* field)
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

void EditorCancels::OnUpdate(int listIdx, EditorInput* field)
{
	std::string& name = field->name;
	auto& item = m_items[listIdx];

	if (name == "command" || name == "move_id") {
		// More complex validation than what is shown in the editors.cpp (cross-field validation)
		EditorInput* commandField = item->identifierMaps["command"];
		EditorInput* moveIdField = item->identifierMaps["move_id"];
		uint64_t command = (uint64_t)strtoll(commandField->buffer, nullptr, 16);

		if (command == m_editor->constants[EditorConstants_GroupedCancelCommand]) {
			int groupId = atoi(moveIdField->buffer);
			moveIdField->errored = groupId >= m_baseWindow->editorTable->groupCancelCount;
		}
		else {
			int moveId = m_baseWindow->ValidateMoveId(moveIdField->buffer);
			moveIdField->errored = moveId == -1;
		}
	}

	BuildItemDetails(listIdx);
}

void EditorCancels::BuildItemDetails(int listIdx)
{
	std::string label;

	auto& item = m_items[listIdx];

	EditorInput* commandField = item->identifierMaps["command"];
	EditorInput* moveIdField = item->identifierMaps["move_id"];
	uint64_t command = (uint64_t)strtoll(commandField->buffer, nullptr, 16);

	if (m_editor->isCommandInputSequence(command)) {
		commandField->flags |= EditorInput_Clickable;
		int inputSequenceId = (command & 0xFFFFFFFF) - m_editor->constants[EditorConstants_InputSequenceCommandStart];

		item->color = MOVEID_INPUT_SEQUENCE;
		commandField->displayName = "edition.cancel.sequence_id";

		int move_id = atoi(moveIdField->buffer);
		int validated_move_id = m_baseWindow->ValidateMoveId(moveIdField->buffer);

		if (m_baseWindow->ValidateMoveId(moveIdField->buffer) == -1) {
			label = std::format("{} / {} / {}: {}", move_id, _("edition.form_list.invalid"), _("edition.input_sequence.window_name"), inputSequenceId);
		}
		else {
			const char* moveName = m_baseWindow->movelist[validated_move_id]->name.c_str();
			label = std::format("{} / {} / {}: {}", move_id, moveName, _("edition.input_sequence.window_name"), inputSequenceId);
		}
	}
	else {
		commandField->displayName = "edition.cancel.command";
		if (commandField->flags & EditorInput_Clickable) {
			commandField->flags -= EditorInput_Clickable;
		}

		int move_id = atoi(moveIdField->buffer);
		if (command == m_editor->constants[EditorConstants_GroupedCancelCommand]) {
			moveIdField->displayName = "edition.cancel.group_id";
			label = std::format("{}: {}", _("edition.grouped_cancel.window_name"), move_id);
			item->color = MOVEID_GROUP_CANCEL;
		}
		else {
			item->color = 0;
			moveIdField->displayName = "edition.cancel.move_id";
			int validated_move_id = m_baseWindow->ValidateMoveId(moveIdField->buffer);
			std::string commandStr = m_editor->GetCommandStr(commandField->buffer);
			if (validated_move_id == -1) {
				label = std::format("{} ({}) / {}", _("edition.form_list.invalid"), move_id, commandStr);
			}
			else
			{
				const char* moveName = m_baseWindow->movelist[validated_move_id]->name.c_str();
				label = std::format("{} / {} / {}", move_id, moveName, commandStr);
			}
		}
	}

	item->itemLabel = label;
}