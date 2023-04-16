#include "InjectionUtils.hpp"

#include <tlhelp32.h>

namespace InjectionUtils
{
	uint64_t GetSelfModuleAddress(std::string moduleName)
	{
		HANDLE moduleSnap;
		MODULEENTRY32 me32{ 0 };
		uint64_t moduleAddress = 0;

		moduleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());

		if (GetLastError() != ERROR_ACCESS_DENIED)
		{
			me32.dwSize = sizeof(MODULEENTRY32);
			if (Module32First(moduleSnap, &me32))
			{
				if (me32.szModule == moduleName) {
					moduleAddress = (uint64_t)me32.modBaseAddr;
				}
				else {
					while (Module32Next(moduleSnap, &me32)) {
						if (me32.szModule == moduleName)
						{
							moduleAddress = (uint64_t)me32.modBaseAddr;
							break;
						}
					}
				}
			}

			CloseHandle(moduleSnap);
		}

		return moduleAddress;
	};
}
