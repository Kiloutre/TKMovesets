#pragma once

#define _(...) Localization::GetText(__VA_ARGS__)

namespace Localization
{
	// Load a translation file for use with Localization::GetText
	void LoadFile(const char* c_langId, bool unloadPrevious = true);

	// Returns a translated string from its corresponding string identifier
	const char* GetText(const char* c_stringId);
}