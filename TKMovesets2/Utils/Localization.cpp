#include <fstream>
#include <map>
#include <format>
#include <sstream>
#include <string>

#include "Localization.hpp"

std::map<std::string, std::string> g_translations;

namespace Localization
{
	void LoadFile(const char* c_langId, bool unloadPrevious)
	{
		// If you want fallback lines (like en_US by default when the current file is lacking an entry), set unloadPrevious to false.
		// I generally don't like this idea though because i want lacking translations to be made clear visually.
		if (unloadPrevious) {
			g_translations.clear();
		}

		std::ifstream infile;
		std::string line;

		infile.open(std::format("Translations/{}.txt", c_langId));
		while (std::getline(infile, line))
		{
			size_t commentStart = line.find_first_of('#');

			size_t idStart = line.find_first_not_of(' ', 0);
			size_t separator = line.find_first_of('=');

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

			std::string key = line.substr(idStart, line.find_first_of(" =", idStart));
			std::string value;
			{
				size_t value_start = line.find_first_not_of(" =", separator);
				if (value_start == std::string::npos) {
					continue;
				}
				value = line.substr(value_start, line.find_last_not_of(" ") + 1 - value_start);
			}

			if (key.length() == 0) {
				continue;
			}
		
			{
				// Translations may contain \n, so we replace them with the actual '\n' character
				size_t newlinePos = 0;
				while ((newlinePos = value.find("\\n", newlinePos)) != std::string::npos)
				{
					value.replace(newlinePos, 2, "\n");
					newlinePos++;
				}
			}

			g_translations[key] = value;
		}
	}

	const char* GetText(const char* c_stringId)
	{
		if (g_translations.find(c_stringId) != g_translations.end()) {
			return g_translations[c_stringId].c_str();
		}
		return c_stringId;
	}
}