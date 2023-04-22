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

	bool compare_memory_string(void* address, const char* bytesString)
	{
		if (address == 0 || bytesString == nullptr) {
			return false;
		}

		char* filteredBytesStr;
		{
			unsigned int spaceCount = 0;
			unsigned int i;
			for (i = 0; bytesString[i]; ++i) {
				if (bytesString[i] == ' ') ++spaceCount;
			}

			unsigned int newLength = i - spaceCount;
			bool isOdd = newLength & 1;
			filteredBytesStr = new char[newLength + isOdd + 1];

			for (i = 0; *bytesString; ++bytesString) {
				if (*bytesString != ' ') {
					filteredBytesStr[i++] = *bytesString;
				}
			}

			if (isOdd) {
				filteredBytesStr[newLength] = '?';
				filteredBytesStr[newLength + 1] = '\0';
			}
			else {
				filteredBytesStr[newLength] = '\0';
			}
		}

		Byte* currentAddress = (Byte*)address;

		char buf[3];
		buf[2] = '\0';

		for (unsigned int i = 0; filteredBytesStr[i]; i += 2)
		{
			// Ignore spaces
			Byte b = *currentAddress;

			char firstChar = filteredBytesStr[i];
			char secondChar = filteredBytesStr[i + 1];
			bool firstCharWildcard = firstChar == '?';
			bool secondCharWildcard = secondChar == '?' || secondChar == '\0';

			if (firstCharWildcard && secondCharWildcard) {
				++currentAddress;
				continue;
			}
			else {
				buf[1] = '\0';
				if (secondCharWildcard) {
					buf[0] = firstChar;
					b >>= 4;
				}
				else {
					buf[0] = secondChar;
					b &= 0xF;
				}
			}

			Byte value = strtol(buf, 0, 16);
			if (b != value) {
				delete[] filteredBytesStr;
				return false;
			}

			++currentAddress;
		}

		delete[] filteredBytesStr;
		return true;
	}
}
