#include <fstream>
#include <map>
#include <sstream>

#include "Localization.hpp"
#include "Helpers.hpp"

#include "constants.h"

// Embedded translation file data
extern "C" const char en_US_txt[];
extern "C" const char fr_FR_txt[];
extern "C" const char de_DE_txt[];
extern "C" const char es_ES_txt[];
extern "C" const char ja_JP_txt[];
extern "C" const char ko_KR_txt[];
// --

const TranslationData g_translation_datas[] = {
	{
		.displayName = "English",
		.locale = "en-US",
		.data = en_US_txt
	},
	{
		.displayName = (const char*)u8"Français",
		.locale = "fr-FR",
		.data = fr_FR_txt,
		// .matchingLocale must contains lowercase-only characters
		.matchingLocale = { "fr-ca", "fr-lu", "fr-ch", "fr"}
	},
	{
		.displayName = (const char*)u8"日本語",
		.locale = "ja-JP",
		.data = ja_JP_txt,
		.matchingLocale = { "ja" }
	},
	{
		.displayName = (const char*)u8"한국인",
		.locale = "ko-KR",
		.data = ko_KR_txt,
		.matchingLocale = { "ko" }
	},
	/*
	{
		.displayName = "Español",
		.locale = "es-ES",
		.data = es_ES_txt,
		.matchingLocale = { "es-ar", "es-bo", "es-cl", "es-co", "es-cr", "es-do", "es-ec", "es-sv", "es-gt", "es-hn", "es-mx", "es-ni", "es-pa", "es-py", "es-pe", "es-pr", "es-uy", "es-ve" }
	},
	{
		.displayName = "Deutsch",
		.locale = "de-DE",
		.data = de_DE_txt,
		.matchingLocale = { "de-at", "de-li", "de-lu", "de-ch" }
	},
	*/
};
//

std::map<FasterStringComp, std::string> g_translations;
unsigned int g_langId = 0;

namespace Localization
{
	bool LoadFile(int locale_index, bool unloadPrevious)
	{
		if (locale_index < 0 || locale_index >= _countof(g_translation_datas)) {
			return false;
		}
		return LoadFile(g_translation_datas[locale_index].locale);
	}

	bool LoadFile(const char* locale, bool unloadPrevious)
	{
		DEBUG_LOG("-- LoadFile(%s) --\n", locale);
		int langId = -1;
		
		// Attempt to find locale in translation list

		std::string lowercaseLocale = locale;
		std::transform(lowercaseLocale.begin(), lowercaseLocale.end(), lowercaseLocale.begin(), tolower);
		for (int i = 0; i < _countof(g_translation_datas); ++i)
		{
			if (_stricmp(g_translation_datas[i].locale, locale) == 0
				|| g_translation_datas[i].matchingLocale.contains(lowercaseLocale))
			{
				langId = i;
				break;
			}
		}

		if (langId == -1) {
			DEBUG_LOG("Translation not found for locale '%s'\n", locale);
			return false;
		}

		// If you want fallback lines (like en_US by default when the current file is lacking an entry), set unloadPrevious to false.
		// I generally don't like this idea though because i want lacking translations to be made clear visually.
		if (unloadPrevious) {
			Clear();
		}

		g_langId = langId;
		const TranslationData* newTranslation = &g_translation_datas[g_langId];

		std::stringstream inputData(newTranslation->data);
		std::string line;

		while (std::getline(inputData, line))
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


		DEBUG_LOG("Loaded translation '%s', id %d.\n", locale, g_langId);
		return true;
	}

	unsigned int GetCurrLangId()
	{
		return g_langId;
	}

	bool HasText(const std::string& c_stringId)
	{
		return g_translations.find(c_stringId) != g_translations.end();
	}

	const char* GetText(const char* c_stringId)
	{
		auto item = g_translations.find(c_stringId);
		if (item != g_translations.end()) {
			return item->second.c_str();
		}
		// If a c_stringId gets freed before being used, this will cause problems
		return c_stringId;
	}

	void Clear()
	{
		g_translations.clear();
	}


	void GetTranslationList(const TranslationData** out_list, unsigned int* out_count)
	{
		*out_list = g_translation_datas;
		*out_count = (int)_countof(g_translation_datas);
	}
}