#pragma once

#define _(...) GetLocalizedText(__VA_ARGS__)

void LoadTranslations(const char* c_langId);
const char* GetLocalizedText(const char* c_stringId...);