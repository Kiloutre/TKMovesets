#include <fstream>
#include <sstream>

#include "Settings.hpp"
#include "FileIO.hpp"
#include "Keybinds.hpp"
#include "Helpers.hpp"

#include "constants.h"

#include <Windows.h>
#include <shlobj.h>

std::map<std::string, std::string> g_settings;
static std::filesystem::path g_settingsPathOverride;

namespace
{
	// TKMSettings.ini next to the exe, where versions before the AppData location kept it.
	std::filesystem::path ExeSettingsPath()
	{
		wchar_t exe[MAX_PATH * 2] = { 0 };
		const DWORD length = GetModuleFileNameW(nullptr, exe, (DWORD)std::size(exe));
		if (length == 0 || length >= std::size(exe)) return SETTINGS_FILE;
		return std::filesystem::path(exe).parent_path() / SETTINGS_FILE;
	}

	std::filesystem::path AppDataSettingsPath()
	{
		PWSTR folder = nullptr;
		std::filesystem::path path;
		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, nullptr, &folder)))
			path = std::filesystem::path(folder) / "TKMovesets2" / SETTINGS_FILE;
		CoTaskMemFree(folder);
		return path;
	}

	// First start with the AppData location: the settings file next to the exe is copied there once (and left in place).
	void MigrateExeSettings(const std::filesystem::path& destination)
	{
		std::error_code error;
		if (std::filesystem::exists(destination, error)) return;
		const auto legacy = ExeSettingsPath();
		if (!std::filesystem::is_regular_file(legacy, error)) return;
		std::filesystem::create_directories(destination.parent_path(), error);
		if (std::filesystem::copy_file(legacy, destination, error))
			DEBUG_LOG("Settings copied from '%S' to '%S'\n", legacy.c_str(), destination.c_str());
		else
			DEBUG_ERR("Cannot copy settings from '%S' to '%S': %s", legacy.c_str(), destination.c_str(), error.message().c_str());
	}
}

namespace Settings
{
	std::filesystem::path FilePath()
	{
		if (!g_settingsPathOverride.empty()) return g_settingsPathOverride;
		const auto appData = AppDataSettingsPath();
		return appData.empty() ? ExeSettingsPath() : appData;
	}

	void SetFilePath(const std::filesystem::path& path)
	{
		g_settingsPathOverride = path;
	}

	const std::map<std::string, std::string>& GetSettingsMap() {
		return g_settings;
	}

	bool LoadFile()
	{
		g_settings.clear();
		Keybinds::ApplyDefaultKeybinds();
		auto& keybinds = Keybinds::GetKeybinds();

		const auto path = FilePath();
		if (g_settingsPathOverride.empty()) MigrateExeSettings(path);
		std::ifstream settingsFile(path);
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

			if (idStart == std::string::npos || idStart >= separator) continue;
			auto keyEnd = line.find_last_not_of(" \t", separator - 1);
			std::string key = line.substr(idStart, keyEnd - idStart + 1);
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
			
			if (key.starts_with("keybind_")) {
				// Load keybind in its own dict
				Keybinds::RegisterKeybind(key, value);
			}
			else {
				// Load setting in dict
				g_settings[key] = value;
			}
		}


		DEBUG_LOG("Loaded settings.");
		return true;
	}

	void SaveSettingsFile()
	{
		std::ostringstream settingsFile;
		
		// Save settings
		for (const auto& [key, value] : g_settings) {
			settingsFile << key << " = " << value << "\n";
		}

		// Save keybinds
		const auto& defaultKeybinds = Keybinds::GetDefaultKeybinds();
		for (const auto& [identifier, keys] : Keybinds::GetKeybinds())
		{
			if (Keybinds::DifferFromDefault(defaultKeybinds, identifier, keys))
			{
				settingsFile << identifier << " = ";

				for (auto cursor = keys.begin(); cursor != keys.end();)
				{
					settingsFile << *cursor;
					std::advance(cursor, 1);
					if (cursor == keys.end()) break;
					settingsFile << ";";
				}
				settingsFile << "\n";
			}
		}
        const auto content = settingsFile.str();
        const auto path = FilePath();
        std::error_code error;
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), error);
        if (!FileIO::WriteAtomically(path, content)) DEBUG_ERR("Cannot save settings to '%S'", path.c_str());

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