#include <fstream>
#include <map>
#include <sstream>

#include "Settings.hpp"
#include "Helpers.hpp"

#include "constants.h"

std::map<std::string, std::string> g_settings;

namespace Settings
{
	const std::map<std::string, std::string>& GetSettingsMap() {
		return g_settings;
	}	

	bool LoadFile()
	{
		std::ifstream settingsFile(SETTINGS_FILE);
		std::string line;

		if (settingsFile.fail()) {
			DEBUG_LOG("No settings file found\n");
			return false;
		}

		while (std::getline(settingsFile, line))
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

			g_settings[key] = value;
		}


		DEBUG_LOG("Loaded settings.");
		return true;
	}

	void SaveSettingsFile()
	{
		std::ofstream settingsFile(SETTINGS_FILE);

		for (const auto& [key, value] : g_settings)
		{
			settingsFile.write(key.c_str(), key.size());
			settingsFile.write(" = ", 3);
			settingsFile.write(value.c_str(), value.size());
			settingsFile.write("\n", 1);
		}
	}

	// -- Getters -- //

	const char* Get(const char* c_stringId, const char* defaultValue)
	{
		auto item = g_settings.find(c_stringId);
		return item == g_settings.end() ? defaultValue : item->second.c_str();
	}

	float Get(const char* c_stringId, float defaultValue)
	{
		auto item = g_settings.find(c_stringId);
		return item == g_settings.end() ? defaultValue : (float)atof(item->second.c_str());
	}

	bool Get(const char* c_stringId, bool defaultValue)
	{
		auto item = g_settings.find(c_stringId);
		if (item == g_settings.end()) {
			return defaultValue;
		}

		const char* setting = item->second.c_str();
		while (*setting == ' ') ++setting;

		if (_strnicmp(setting, "true", 4) == 0) {
			// Returns true if string starts with "TRUE", case insensitive
			return true;
		}

		if (_strnicmp(setting, "false", 4) == 0) {
			// Returns false if string starts with "FALSE", case insensitive
			return false;
		}

		if (atoi(setting) || (*setting && *setting != '0')) {
			// Returns true if string is non-zero
			return true;
		}

		return false;
	}

	// -- Setters -- //
	
	void Set(const char* c_stringId, const char* value)
	{
		g_settings[c_stringId] = value;
		SaveSettingsFile();
	}

	void Set(const char* c_stringId, bool value)
	{
		g_settings[c_stringId] = value ? "True" : "False";
		SaveSettingsFile();
	}

	void Set(const char* c_stringId, float value)
	{
		char buf[64];
		sprintf_s(buf, sizeof(buf), "%.7g", value);
		g_settings[c_stringId] = buf;
		SaveSettingsFile();
	}

	void Set(const char* c_stringId, int value)
	{
		char buf[64];
		sprintf_s(buf, sizeof(buf), "%d", value);
		g_settings[c_stringId] = buf;
		SaveSettingsFile();
	}

	void Set(const char* c_stringId, unsigned int value)
	{
		char buf[64];
		sprintf_s(buf, sizeof(buf), "%u", value);
		g_settings[c_stringId] = buf;
		SaveSettingsFile();
	}
}