#include <fstream>
#include <time.h>
#include <chrono>
#include <codecvt>
#include <cctype>

#include "helpers.hpp"

#include "constants.h"
#include "GameTypes.h"
#include "MovesetStructs.h"

// -- -- //


namespace ByteswapHelpers
{
	void SWAP_INT16(void* x)
	{
		auto val = DEREF_INT16(x);
		DEREF_INT16(x) = BYTESWAP_INT16(val);
	}

	void SWAP_INT32(void* x)
	{
		auto val = DEREF_INT32(x);
		DEREF_INT32(x) = BYTESWAP_INT32(val);
	}

	void SWAP_INT64(void* x)
	{
		auto val = DEREF_INT64(x);
		DEREF_INT64(x) = BYTESWAP_INT64(val);
	}
}


namespace Helpers
{
	std::string to_utf8(const std::wstring& ws)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
		return converter.to_bytes(ws);
	}

	std::wstring to_unicode(const std::string& s)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
		return converter.from_bytes(s);
	}

	std::string wstring_to_string(const std::wstring& ws)
	{
		return std::string(ws.begin(), ws.end());
	}

	std::wstring string_to_wstring(const std::string& s)
	{
		return std::wstring(s.begin(), s.end());
	}

	uint64_t align8Bytes(uint64_t value)
	{
		// If any of the first 3 bits are set, is not divisble by 8
		return (value & 0x7) == 0 ? value : value + (8 - value & 7);
	}

	void align8Bytes(std::ofstream& file)
	{
		uint64_t cursor = (uint64_t)file.tellp();
		if ((cursor & 0x7) != 0)
		{
			// Pad file with 0xDD for ease of debug
			char b[7] = { (char)0xDD, (char)0xDD, (char)0xDD, (char)0xDD, (char)0xDD, (char)0xDD, (char)0xDD };
			file.write(b, 8 - cursor & 7);
		}
	}

	uint32_t CalculateCrc32(const std::vector<std::pair<Byte*, uint64_t>>& blocks)
	{
		uint32_t crc32 = 0;
		uint32_t table[256];

		Helpers::crc32_generate_table(table);
		for (size_t i = 0; i < blocks.size(); ++i) {
			char* blockData = (char*)blocks[i].first;
			uint64_t blockSize = blocks[i].second;

			crc32 = Helpers::crc32_update(table, crc32, blockData, blockSize);
#ifdef BUILD_TYPE_DEBUG
			uint32_t single_crc32 = Helpers::crc32_update(table, 0, blockData, blockSize);
			DEBUG_LOG("(CRC32 = %x) - Block %llx (size %llx)\n", single_crc32, (uint64_t)blockData, blockSize);
#endif
		}
		return crc32;
	}

	uint32_t CalculateCrc32(const Byte* data, uint64_t size)
	{
		uint32_t crc32 = 0;
		uint32_t table[256];

		Helpers::crc32_generate_table(table);
		crc32 = Helpers::crc32_update(table, crc32, (char*)data, size);

		return crc32;
	}

	std::string formatDateTime(uint64_t date, bool path_compatible, bool include_seconds)
	{
		time_t     now = date;
		struct tm  tstruct;
		char       buf[20];

		if (localtime_s(&tstruct, &now) == 0) {
			const char* time_format;
			if (include_seconds) {
				time_format = path_compatible ? "%d-%m-%y_%Hh%M_%Ss" : "%d/%m/%Y %H:%M:%S";
			}
			else {
				time_format = path_compatible ? "%d-%m-%y_%Hh%M" : "%d/%m/%Y %H:%M";
			}

			if (strftime(buf, sizeof(buf), time_format, &tstruct) > 0) {
				return std::string(buf);
			}
		}
		return std::string();
	}

	uint64_t getCurrentTimestamp()
	{
		return duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	void convertPtrsToOffsets(void* listAddr, uint64_t to_substract, uint64_t struct_size, uint64_t amount, bool isT8)
	{
		uint64_t listCursor = (uint64_t)listAddr;
		while (amount-- > 0)
		{
			uint64_t* structMember = (uint64_t*)listCursor;
			*structMember -= to_substract;
			listCursor += struct_size;
			if (isT8) {
				listCursor += 8;
				isT8 = false;
			}
		}
	}

	std::string getMovesetNameFromFilename(const std::wstring& filename)
	{
		size_t lastSlash = filename.find_last_of(L"/\\");
		std::wstring tmp;

		if (lastSlash == std::string::npos) {
			return to_utf8(filename.substr(0, filename.size() - strlen(MOVESET_FILENAME_EXTENSION)));
		}
		else {
			return to_utf8(filename.substr(lastSlash + 1, filename.size() - lastSlash - strlen(MOVESET_FILENAME_EXTENSION) - 1));
		}
	}

	std::wstring getMovesetWNameFromFilename(const std::wstring& filename)
	{
		size_t lastSlash = filename.find_last_of(L"/\\");
		std::wstring tmp;

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

	bool fileExists(const wchar_t* name) {
		struct _stat buffer;
		return (_wstat(name, &buffer) == 0);
	}

	bool fileExists(const char* name) {
		struct stat buffer;
		return (stat(name, &buffer) == 0);
	}

	uint32_t crc32_update(const uint32_t(&table)[256], uint32_t initial, const void* buf, size_t len)
	{
		uint32_t c = initial ^ 0xFFFFFFFF;
		const uint8_t* u = static_cast<const uint8_t*>(buf);
		for (size_t i = 0; i < len; ++i) {
			c = table[(c ^ u[i]) & 0xFF] ^ (c >> 8);
		}
		return c ^ 0xFFFFFFFF;
	}

	void crc32_generate_table(uint32_t(&table)[256])
	{
		const uint32_t polynomial = 0xEDB88320;
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

	bool VersionGreater(const char* version1, const char* version2)
	{
		while (*version1 && *version2)
		{
			int number1 = -1;
			int number2 = -1;

			if (isdigit((unsigned char)*version1)) {
				number1 = atoi(version1);
				while (isdigit((unsigned char)*version1)) ++version1;
			}

			if (isdigit((unsigned char)*version2)) {
				number2 = atoi(version2);
				while (isdigit((unsigned char)*version2)) ++version2;

			}

			if (number1 != -1 && number2 != -1)
			{
				// Both are numbers
				if (number1 != number2) {
					// Numbers are different, return the true if v1 is greater
					return number1 > number2;
				}
			}
			else if (number1 != -1)
			{
				// Only string 1 is a number, making it greater
				return true;
			}
			else if (number2 != -1)
			{
				// Only string 2 is a number, making it greater than v1
				return false;
			}
			else
			{
				// Neither are numbers
				if (*version1 != *version2) {
					// Different characters, return true if v1 is greater in the ascii table
					return *version1 > *version2;
				}

				// Pass to the next characters if we haven't already reached the end 
				if (*version1) ++version1;
				if (*version2) ++version2;
			}

		}
		return *version1 > *version2;
	}


	unsigned int get_memory_string_length(const char* bytesString)
	{
		unsigned int size = 0;
		while (*bytesString != '\0')
		{
			while (*bytesString == ' ' || !isprint(*bytesString)) ++bytesString; // Ignore spaces
			if (*bytesString == '\0') break;

			// first byte
			// second byte

			size += 2;

			++bytesString;
			if (*bytesString != '\0') ++bytesString;
		}
		return size;
	}

	bool compare_memory_string(void* address, const char* bytesString)
	{
		if (address == 0 || bytesString == nullptr) {
			return false;
		}

		Byte* currentAddress = (Byte*)address;

		char buf[3]{ 0, 0, 0 };

		while (*bytesString != '\0')
		{
			while (*bytesString == ' ' || !isprint(*bytesString)) ++bytesString; // Ignore spaces
			if (*bytesString == '\0') break;

			Byte b = *currentAddress;

			char firstChar = bytesString[0];
			char secondChar = *bytesString ? bytesString[1] : '?';

			bool firstCharWildcard = firstChar == '?';
			bool secondCharWildcard = secondChar == '?' || secondChar == '\0';

			if (firstCharWildcard && secondCharWildcard) {
				++currentAddress;
				++bytesString;
				if (*bytesString != '\0') ++bytesString;
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

			Byte value = (Byte)strtol(buf, 0, 16);
			if (b != value)  return false;

			++currentAddress;
			++bytesString;
			if (*bytesString != '\0') ++bytesString;
		}
		return true;
	}

	Byte* ReadMovesetFile(const std::wstring& filename, uint64_t& size_out)
	{
		std::ifstream file(filename, std::ios::binary);
		file.seekg(0, std::ios::end);
		size_out = file.tellg();

		if (size_out < sizeof(TKMovesetHeader)) {
			throw MovesetFile_BadSize();
		}

		Byte* moveset = (Byte*)malloc(size_out);
		if (moveset == nullptr) {
			throw MovesetFile_AllocationError();
		} else {
			file.seekg(0, std::ios::beg);
			file.read((char*)moveset, size_out);


			if (!((TKMovesetHeader*)moveset)->ValidateHeader()) {
				free(moveset);
				throw MovesetFile_InvalidHeader();
			}
		}

		return moveset;
	}

	bool is_string_digits(const char* str)
	{
		while (*str == ' ') ++str;

		while (true)
		{
			char c = *str;
			if (c == '\0') return true;
			if (c < '0' || c > '9') return false;
			++str;
		}

		while (*str == ' ') ++str;
		return *str == '\0';
	}
}
