#pragma once

#include <string>
#include <windows.h>

namespace InjectionUtils
{
	// Returns the module address of the given module name inside of the current process the DLL is injected in
	void GetSelfModuleInfos(std::string moduleName, uint64_t& moduleAddr_out, uint64_t& moduleSize_out);
}
