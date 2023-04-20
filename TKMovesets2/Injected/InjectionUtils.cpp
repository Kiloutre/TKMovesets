#include "InjectionUtils.hpp"

#include <tlhelp32.h>

namespace InjectionUtils
{
	void GetSelfModuleInfos(std::string moduleName, uint64_t& moduleAddr_out, uint64_t& moduleSize_out)
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
					moduleAddr_out = (uint64_t)me32.modBaseAddr;
					moduleSize_out = (uint64_t)me32.modBaseSize;
				}
				else {
					while (Module32Next(moduleSnap, &me32)) {
						if (me32.szModule == moduleName)
						{
							moduleAddr_out = (uint64_t)me32.modBaseAddr;
							moduleSize_out = (uint64_t)me32.modBaseSize;
							break;
						}
					}
				}
			}

			CloseHandle(moduleSnap);
		}
	};
}
