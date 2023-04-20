#pragma once

#define _(...) Localization::GetText(__VA_ARGS__)

struct TranslationData
{
	const char* displayName;
	const char* locale;
	const char* data;
	const size_t* data_size;
};

namespace Localization
{
	// Load a translation file for use with Localization::GetText(). Returns false if locale not found.
	bool LoadFile(const char* c_langId, bool unloadPrevious = true);

	// Returns a translated string from its corresponding string identifier. To be used only in the main thread.
	const char* GetText(const char* c_stringId);

	// Returns the current lang id. To be used only in the main thread
	unsigned int GetCurrLangId();

	// Clear every translation form the dictionnary
	void Clear();

	// Sends the array of translation and its size to the passed variables
	void GetTranslationList(TranslationData** out_list, unsigned int* out_count);
}