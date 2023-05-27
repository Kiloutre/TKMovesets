#pragma once

#include <set>
#include <string>

#define _(...) Localization::GetText(__VA_ARGS__)

struct TranslationData
{
	const char* displayName;
	const char* locale;
	const char* data;
	// Set of locale that will trigger a match. Must be lowercase only.
	std::set<std::string> matchingLocale;
};

namespace Localization
{
	// Load a translation file for use with Localization::GetText(). Returns false if locale not found.
	bool LoadFile(const char* c_langId, bool unloadPrevious = true);

	// Returns true if the specified translation exists
	bool HasText(const std::string& c_stringId);

	// Returns a translated string from its corresponding string identifier. To be used only in the main thread.
	const char* GetText(const char* c_stringId);

	// Returns the current lang id. To be used only in the main thread
	unsigned int GetCurrLangId();

	// Clear every translation form the dictionnary
	void Clear();

	// Sends the array of translation and its size to the passed variables
	void GetTranslationList(const TranslationData** out_list, unsigned int* out_count);
}