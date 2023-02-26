#pragma once

#define _(...) GetLocalizedText(__VA_ARGS__)

const char* GetLocalizedText(const char* string_id...);