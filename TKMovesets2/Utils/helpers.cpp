#include <fstream>
#include <time.h>
#include <chrono>
#include <codecvt>
#include <cctype>

#include "helpers.hpp"

#include "constants.h"
#include "GameTypes.h"

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

	uint64_t GetAnimationSize(Byte* anim)
	{
		uint16_t animType = *(uint16_t*)anim;
		if ((animType & 0xFF) == 0) {
			animType = BYTESWAP_INT16(animType);
		}

		if (animType == 0xC8) {
			return getC8AnimSize(anim);
		}
		else if (animType == 0x64) {
			return get64AnimSize(anim);
		}

		// Invalid animation type
		DEBUG_LOG("Animation address %llx does not have a valid animation type. First four bytes: %x\n", (uint64_t)anim, *(uint32_t*)anim);
		throw;
	}

	uint64_t getC8AnimSize(Byte* anim)
	{
		bool isSwapped = *anim == 0;

		uint8_t bone_count = anim[isSwapped ? 3 : 2];
		uint32_t header_size = bone_count * 0x4 + 0x8;
		uint32_t frame_size = bone_count * 0x4 * 3;

		uint32_t length = *(uint32_t*)&anim[4];
		if (isSwapped) {
			length = BYTESWAP_INT32(length);
		}

		return (int64_t)header_size + (int64_t)frame_size * (int64_t)length;
	}

	uint64_t get64AnimSize(Byte* anim)
	{
		bool isSwapped = *anim == 0;
		return isSwapped ? get64AnimSize_BigEndian(anim) : get64AnimSize_LittleEndian(anim);
	}

	uint64_t get64AnimSize_BigEndian(Byte* anim)
	{
		// Do all calculations in uint64_t that way i don't have to pay attention to possible overflows

		uint64_t boneCount = *(uint16_t*)&anim[2];
		boneCount = BYTESWAP_INT16(boneCount);

		uint64_t postBoneDescriptor_offset = (4 + boneCount * sizeof(uint16_t));
		Byte* anim_postBoneDescriptorAddr = (anim + postBoneDescriptor_offset);

		uint64_t animLength = *(uint16_t*)&anim_postBoneDescriptorAddr[0];
		uint64_t __unknown__ = *(uint16_t*)&anim_postBoneDescriptorAddr[4];
		animLength = BYTESWAP_INT16(animLength);
		__unknown__ = BYTESWAP_INT16(__unknown__);

		uint64_t vv73 = 2 * ((4 * __unknown__ + 6) / 2);
		uint64_t aa4 = 6 * (__unknown__ + boneCount);

		Byte* animPtr = anim_postBoneDescriptorAddr + vv73 + aa4;

		unsigned int baseFrame = (unsigned int)animLength - (animLength >= 2 ? 2 : 1);
		unsigned int keyframe = baseFrame / 16;
		unsigned int _v56_intPtr = *(unsigned int*)&animPtr[4 * (uint64_t)keyframe];
		_v56_intPtr = BYTESWAP_INT32(_v56_intPtr);

		Byte* animPtr_2 = animPtr + _v56_intPtr;
		int lastArg_copy = (int)boneCount;

		do
		{
			for (int i = 0; i < 3; ++i)
			{
				Byte v58 = *animPtr_2;
				int offsetStep = v58 / 4;
				animPtr_2 += offsetStep;
			}
		} while (--lastArg_copy != 0);

		return (uint64_t)animPtr_2 - (uint64_t)anim;
	}

	uint64_t get64AnimSize_LittleEndian(Byte* anim)
	{
		// Do all calculations in uint64_t that way i don't have to pay attention to possible overflows

		uint64_t boneCount = *(uint16_t*)&anim[2];

		uint64_t postBoneDescriptor_offset = (4 + boneCount * sizeof(uint16_t));
		Byte* anim_postBoneDescriptorAddr = (anim + postBoneDescriptor_offset);

		uint64_t animLength = *(uint16_t*)&anim_postBoneDescriptorAddr[0];
		uint64_t __unknown__ = *(uint16_t*)&anim_postBoneDescriptorAddr[4];

		uint64_t vv73 = 2 * ((4 * __unknown__ + 6) / 2);
		uint64_t aa4 = 6 * (__unknown__ + boneCount);

		Byte* animPtr = anim_postBoneDescriptorAddr + vv73 + aa4;

		unsigned int baseFrame = (unsigned int)animLength - (animLength >= 2 ? 2 : 1);
		unsigned int keyframe = baseFrame / 16;
		unsigned int _v56_intPtr = *(unsigned int*)&animPtr[4 * (uint64_t)keyframe];

		Byte* animPtr_2 = animPtr + _v56_intPtr;
		int lastArg_copy = (int)boneCount;

		do
		{
			for (int i = 0; i < 3; ++i)
			{
				Byte v58 = *animPtr_2;
				int offsetStep = v58 / 4;
				animPtr_2 += offsetStep;
			}
		} while (--lastArg_copy != 0);

		return (uint64_t)animPtr_2 - (uint64_t)anim;
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

	uint32_t CalculateCrc32(std::vector<std::pair<Byte*, uint64_t>>& blocks)
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

	std::string formatDateTime(uint64_t date) {
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

	uint64_t getCurrentTimestamp()
	{
		return duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
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

	bool remoteVersionGreater(const char* otherVersion)
	{
		return VersionGreater(PROGRAM_VERSION, otherVersion);
	}

	bool VersionGreater(const char* version1, const char* version2)
	{
		const char* currentVersion = PROGRAM_VERSION;
		return strcmp(version1, version2) < 0;
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
}
