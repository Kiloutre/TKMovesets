#pragma once

#include <string>
#include <map>

#ifndef MODULE_ENTRY_DEF
#define MODULE_ENTRY_DEF
struct moduleEntry
{
	uint64_t address;
	std::string name;
	std::wstring path;
	uint64_t size;
};
#endif

namespace InjectionUtils
{
	// Returns the list of modules and stores the main module address & size inside of the provided variables.
	std::map<std::string, moduleEntry> GetModuleList(std::string mainModuleName, uint64_t& moduleAddr_out, uint64_t& moduleSize_out);

	// Compares a memory area with a memory string such as '00 F7 E1 45 0? 45 ?? ?? 89'
	bool compare_memory_string(void* address, const char* bytesString);
}