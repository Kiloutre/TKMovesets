#include <map>
#include <time.h>

#include "helpers.hpp"

#include "constants.h"
#include "GameAddresses.h"

namespace Helpers
{
	std::string currentDateTime() {
		time_t     now = time(0);
		struct tm  tstruct;
		char       buf[20];

		localtime_s(&tstruct, &now);
		strftime(buf, sizeof(buf), "%H:%M %d/%m/%Y", &tstruct);
		return std::string(buf);
	}

	void ConvertPtrsToOffsets(void* listAddr, uint64_t to_substract, uint64_t struct_size, uint64_t amount)
	{
		uint64_t listCursor = (uint64_t)listAddr;
		while (amount-- > 0)
		{
			uint64_t* structMember = (uint64_t*)listCursor;
			*structMember -= to_substract;
			listCursor += struct_size;
		}
	}

	void ConvertPtrsToOffsets(void* listAddr, std::map<gameAddr, uint64_t> m, uint64_t struct_size, uint64_t amount)
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

	bool endsWith(std::string_view str, std::string_view suffix)
	{
		return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
	}

	std::string getMovesetNameFromFilename(std::string filename)
	{
		return filename.substr(strlen(MOVESET_DIRECTORY) + 1, filename.size() - strlen(MOVESET_DIRECTORY) - strlen(MOVESET_FILENAME_EXTENSION) - 1);
	}
}
