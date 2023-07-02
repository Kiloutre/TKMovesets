#pragma once

#include <map>
#include <set>
#include <string>
#include <imgui.h>

namespace Settings
{
	// Load a translation file for use with Localization::GetText(). Returns false if locale not found.
	bool LoadFile();

	const std::map<std::string, std::string>& GetSettingsMap();
	// Returns a string setting
	const char* Get(const char* c_stringId, const char* defaultValue);
	// Returns a float setting
	float Get(const char* c_stringId, float defaultValue);
	// Returns a bool setting
	bool Get(const char* c_stringId, bool defaultValue);
	// Returns an integer string
	template<typename T>
	T Get(const char* c_stringId, T defaultValue)
	{
		static_assert(std::is_integral<T>::value, "Integral required.");
		auto& settings = GetSettingsMap();
		auto item = settings.find(c_stringId);
		return item == settings.end() ? defaultValue : (T)atoll(item->second.c_str());
	}

	// Sets an int setting
	void Set(const char* c_stringId, int value);
	// Sets an unsigned setting
	void Set(const char* c_stringId, unsigned int value);
	// Sets a float setting
	void Set(const char* c_stringId, float value);
	// Sets a boolean setting
	void Set(const char* c_stringId, bool value);
	// Sets a string setting
	void Set(const char* c_stringId, const char* value);

	// Save all the settings in the settings file (rewriting it from scratch)
	void SaveSettingsFile();
}