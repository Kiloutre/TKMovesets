#pragma once

#define _(...) Localization::GetText(__VA_ARGS__)

namespace Localization
{
	void LoadFile(const char* c_langId);
	const char* GetText(const char* c_stringId...);
}