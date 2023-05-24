#include <windows.h>
#include <tlhelp32.h>

#include "InjectionUtils.hpp"
#include "Helpers.hpp"

namespace InjectionUtils
{
	std::map<std::string, moduleEntry> GetModuleList(std::string mainModuleName, uint64_t& moduleAddr_out, uint64_t& moduleSize_out)
	{
		HANDLE moduleSnap;
		MODULEENTRY32W me32{ 0 };

		std::map<std::string, moduleEntry> modules;

		moduleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());

		if (GetLastError() != ERROR_ACCESS_DENIED)
		{
			me32.dwSize = sizeof(me32);
			if (Module32FirstW(moduleSnap, &me32))
			{
				{
					auto moduleName = Helpers::wstring_to_string(me32.szModule);
					modules[moduleName] = {
						.address = (uint64_t)me32.modBaseAddr,
						.name = moduleName,
						.path = std::wstring(me32.szExePath),
						.size = (uint64_t)me32.modBaseSize
					};

					if (moduleName == mainModuleName) {
						auto& lastAddedModule = modules[moduleName];
						moduleAddr_out = lastAddedModule.address;
						moduleSize_out = lastAddedModule.size;
					}
				}

				while (Module32NextW(moduleSnap, &me32))
				{
					auto moduleName = Helpers::wstring_to_string(me32.szModule);
					modules[moduleName] = {
						.address = (uint64_t)me32.modBaseAddr,
						.name = moduleName,
						.path = std::wstring(me32.szExePath),
						.size = (uint64_t)me32.modBaseSize
					};

					if (moduleName == mainModuleName) {
						auto& lastAddedModule = modules[moduleName];
						moduleAddr_out = lastAddedModule.address;
						moduleSize_out = lastAddedModule.size;
					}
				}
			}
		}

		CloseHandle(moduleSnap);
		return modules;
	}
}
