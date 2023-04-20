#include <fstream>
#include <map>
#include <format>
#include <sstream>
#include <string>

#include "Games.hpp"
#include "EditorLabel.hpp"
#include "Helpers.hpp"

#include "constants.h"

// -- Private methods -- //

void EditorLabel::LoadFile(const char* dataString, const char* name)
{
	std::string filename;
	filename = std::format("{}/editor_{}_{}.txt", INTERFACE_DATA_DIR, dataString, name);


    if (!Helpers::fileExists(filename.c_str())) {
        return;
    }


	std::ifstream infile;
	std::string line;

	infile.open(filename.c_str());

	while (std::getline(infile, line))
	{
		size_t commentStart = line.find_first_of('#');

		size_t idStart = line.find_first_not_of(' ', 0);
		size_t separator = line.find_first_of(',');

		if (separator == std::string::npos) {
			// Not a 'key = value' entry
			continue;
		}
		if (commentStart != std::string::npos)
		{
			if (commentStart < separator) continue;
			// We remove everything in the line before the comment
			line = line.substr(0, commentStart);
		}

		std::string keyStr = line.substr(idStart, line.find_first_of(" ,", idStart));
		std::string value;
		{
			size_t value_start = line.find_first_not_of(" ,", separator);
			if (value_start == std::string::npos) {
				continue;
			}
			value = line.substr(value_start, line.find_last_not_of(" ") + 1 - value_start);
		}

		if (keyStr.length() == 0) {
			continue;
		}

		{
			// May contain \n, so we replace them with the actual '\n' character
			size_t newlinePos = 0;
			while ((newlinePos = value.find("\\n", newlinePos)) != std::string::npos)
			{
				value.replace(newlinePos, 2, "\n");
				newlinePos++;
			}
		}

		int key = (int)strtoll(keyStr.c_str(), nullptr, 0);
		m_labels[key] = value;
	}
}

// -- Public methods -- //

EditorLabel::EditorLabel(const GameInfo* gameInfo)
{
	const char* dataString = gameInfo->dataString;

	LoadFile(dataString, "properties");
	LoadFile(dataString, "requirements");
}

const char* EditorLabel::GetText(int id)
{
	if (m_labels.find(id) != m_labels.end()) {
		return m_labels[id].c_str();
	}
	return nullptr;
}