#pragma once

#define _(...) Localization::GetText(__VA_ARGS__)

namespace Localization
{
	// Load a translation file for use with Localization::GetText
	void LoadFile(const char* c_langId, bool unloadPrevious = true);

	// Returns a translated string from its corresponding string identifier. To be used only in the main thread.
	const char* GetText(const char* c_stringId);

	// Returns the current lang id. To be used only in the main thread
	const char* GetCurrLangId();

	// Clear every translation form the dictionnary
	void Clear();
}