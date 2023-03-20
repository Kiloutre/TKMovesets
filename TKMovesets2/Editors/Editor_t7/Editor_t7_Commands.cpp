#include "Editor_t7.hpp"

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

static std::string GetCommandString(uint64_t command)
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

bool EditorT7::isCommandInputSequence(uint64_t command)
{
	return (command & 0xFFFFFFFF) >= constants[EditorConstants_InputSequenceCommandStart];
}

std::string EditorT7::GetCommandStr(const char* commandBuf)
{
	uint64_t command = strtoll(commandBuf, nullptr, 16);
	return GetCommandString(command);
}

std::string EditorT7::GetCommandStr(const char* direction, const char* button)
{
	uint64_t command = strtoll(direction, nullptr, 16);
	command = (command << 32) | strtoll(button, nullptr, 16);

	return GetCommandString(command);
}