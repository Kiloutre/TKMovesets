#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorGroupedCancels.hpp"
#include "Localization.hpp"

const std::vector<std::string> cg_directionBitLabels = {
	"D/B",
	"D",
	"D/F",
	"B",
	"N",
	"F",
	"U/B",
	"U",
	"U/F",
};

// -- Static helpers -- //

static std::string GetCancelCommandStr(uint64_t command)
{
	int inputBits = command >> 32;
	int directionBits = command & 0xFFFFFFFF;

	if (command == 0x8000) {
		return "AUTO";
	}

	if (directionBits >= 0x800d && directionBits <= 0x8FFF) {
		// Input sequence
		return std::string();
	}

	std::string retVal;

	// Directions
	{
		for (int i = 1; i < 10; ++i)
		{
			if (directionBits & (1 << i)) {
				if (retVal.size() != 0) {
					retVal += "|";
				}
				retVal += cg_directionBitLabels[i - 1];
			}
		}

		if (directionBits >> 10 || directionBits & 1) {
			if (retVal.size() != 0) {
				retVal += "|";
			}
			retVal += "???";
		}
	}

	// Inputs
	{
		std::string inputs;
		for (int i = 0; i < 4; ++i)
		{
			if (inputBits & (1 << (i))) {
				inputs += "+" + std::to_string(i + 1);
			}
		}
		if (inputBits & (1 << 4)) {
			inputs += "+RA";
		}
		if (inputs.size() > 0) {
			retVal += inputs.substr(retVal.size() == 0 ? 1 : 0);
		}
	}

	return retVal.size() == 0 ? retVal : "[ " + retVal + " ]";
}

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

	if ((command & 0xFFFFFFFF) >= m_editor->constants[EditorConstants_InputSequenceCommandStart]) {
		commandField->flags |= EditorInput_Clickable;
		int inputSequenceId = (command & 0xFFFFFFFF) - m_editor->constants[EditorConstants_InputSequenceCommandStart];
		label = std::format("{}: {}", _("edition.input_sequence.window_name"), inputSequenceId);
	}
	else {
		if (commandField->flags & EditorInput_Clickable) {
			commandField->flags -= EditorInput_Clickable;
		}

		int move_id = atoi(moveIdField->buffer);

		int validated_move_id = m_baseWindow->ValidateMoveId(moveIdField->buffer);
		std::string commandStr = GetCancelCommandStr(command);
		if (validated_move_id == -1) {
			label = std::format("{} ({}) / {}", _("edition.form_list.invalid"), move_id, commandStr);
		}
		else
		{
			const char* moveName = m_baseWindow->movelist[validated_move_id]->name.c_str();
			label = std::format("{} / {} / {}", move_id, moveName, commandStr);
		}
	}

	item->itemLabel = label;
}