#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorInputStruct.hpp"
#include "Localization.hpp"

// -- Static helper -- //

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

	return retVal;
}

// -- Public methods -- //

EditorInputStruct::EditorInputStruct(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow, int listSize)
{
	windowType = EditorWindowType_Input;
	m_baseWindow = baseWindow;
	m_listSize = listSize;
	InitForm(windowTitleBase, t_id, editor);
}

// -- Private methods-- //

void EditorInputStruct::BuildItemDetails(int listIdx)
{
	auto& identifierMaps = m_items[listIdx]->identifierMaps;
	
	uint64_t command = strtoll(identifierMaps["direction"]->buffer, nullptr, 16);
	command = (command << 32) | strtoll(identifierMaps["button"]->buffer, nullptr, 16);

	m_items[listIdx]->itemLabel = GetCancelCommandStr(command);
}