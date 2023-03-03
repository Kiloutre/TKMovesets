#include <map>
#include <time.h>
#include <cctype>

#include "helpers.hpp"

#include "constants.h"
#include "GameAddresses.h"

namespace Helpers
{
	std::string currentDateTime(uint64_t date) {
		time_t     now = date;
		struct tm  tstruct;
		char       buf[20];

		localtime_s(&tstruct, &now);
		strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", &tstruct);
		return std::string(buf);
	}

	void convertPtrsToOffsets(void* listAddr, uint64_t to_substract, uint64_t struct_size, uint64_t amount)
	{
		uint64_t listCursor = (uint64_t)listAddr;
		while (amount-- > 0)
		{
			uint64_t* structMember = (uint64_t*)listCursor;
			*structMember -= to_substract;
			listCursor += struct_size;
		}
	}

	void convertPtrsToOffsets(void* listAddr, std::map<gameAddr, uint64_t> m, uint64_t struct_size, uint64_t amount)
	{
		uint64_t listCursor = (uint64_t)listAddr;
		while (amount-- > 0)
		{
			uint64_t* structMember = (gameAddr*)listCursor;
			gameAddr addr = (gameAddr)*structMember;
			if (m.find(addr) != m.end()) {
				*structMember = m[addr];
			}
			else {
				*structMember -= (uint64_t)-1;
			}
			listCursor += struct_size;
		}
	}

	bool endsWith(std::string str, std::string suffix)
	{
		if (str.length() < suffix.length())
			return false;
		size_t i = str.length() - suffix.length();

		for (char c : suffix)
		{
			if (c != str[i]) {
				return false;
			}
			++i;
		}

		return true;
	}

	std::string getMovesetNameFromFilename(std::string filename)
	{
		size_t lastSlash = filename.find_last_of("/\\");
		if (lastSlash == std::string::npos) {
			return filename.substr(0, filename.size() - strlen(MOVESET_FILENAME_EXTENSION));
		}
		else {
			return filename.substr(lastSlash + 1, filename.size() - lastSlash - strlen(MOVESET_FILENAME_EXTENSION) - 1);
		}
	}

	bool isHeaderStringMalformated(const char* str, size_t size)
	{
		size_t i = 0;

		while (i < size && str[i] != '\0') {
			if (isalnum((unsigned char)str[i]) == 0 && strchr(MOVESET_HEADER_STRING_CHARSET, str[i]) == nullptr) {
				return true;
			}
			++i;
		}

		if (i == 0) {
			// Empty string
			return true;
		}
		return str[i] != '\0'; // Ensure last char is a nullbyte
	}

	bool fileExists(const char* name) {
		struct stat buffer;
		return (stat(name, &buffer) == 0);
	}
}
