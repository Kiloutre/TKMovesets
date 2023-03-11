#include <map>
#include <time.h>
#include <cctype>
#include <fstream>

#include "helpers.hpp"

#include "constants.h"
#include "GameAddresses.h"

namespace Helpers
{
	uint64_t align8Bytes(uint64_t value)
	{
		// If any of the first 3 bits are set, is not divisble by 8
		return (value & 0x3) == 0 ? value : value + (8 - value % 8);
	}

	void align8Bytes(std::ofstream& file)
	{
		uint64_t cursor = (uint64_t)file.tellp();
		if ((cursor & 0x3) != 0)
		{
			// pad file with 0
			char b[8]{ 0 };
			file.write(b, 8 - cursor % 8);
		}
	}


	std::string currentDateTime(uint64_t date) {
		time_t     now = date;
		struct tm  tstruct;
		char       buf[20];

		if (localtime_s(&tstruct, &now) == 0) {
			if (strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", &tstruct) > 0) {
				return std::string(buf);
			}
		}
		return std::string();
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

	bool startsWith(std::string str, std::string prefix)
	{
		if (str.length() < prefix.length())
			return false;

		for (size_t i = 0; prefix[i]; ++i)
		{
			if (prefix[i] != str[i]) {
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


	uint32_t crc32_update(uint32_t(&table)[256], uint32_t initial, const void* buf, size_t len)
	{
		uint32_t c = initial ^ 0xFFFFFFFF;
		const uint8_t* u = static_cast<const uint8_t*>(buf);
		for (size_t i = 0; i < len; ++i)
		{
			c = table[(c ^ u[i]) & 0xFF] ^ (c >> 8);
		}
		return c ^ 0xFFFFFFFF;
	}

	void crc32_generate_table(uint32_t(&table)[256])
	{
		uint32_t polynomial = 0xEDB88320;
		for (uint32_t i = 0; i < 256; i++)
		{
			uint32_t c = i;
			for (size_t j = 0; j < 8; j++)
			{
				if (c & 1) {
					c = polynomial ^ (c >> 1);
				}
				else {
					c >>= 1;
				}
			}
			table[i] = c;
		}
	}
}
