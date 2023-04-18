#include <map>
#include <time.h>
#include <cctype>
#include <fstream>
#include <chrono>

#include "helpers.hpp"

#include "constants.h"
#include "GameAddresses.h"

namespace Helpers
{
	std::string wstring_to_string(const std::wstring& ws)
	{
		return std::string(ws.begin(), ws.end());
	}

	uint64_t GetAnimationSize(Byte* anim)
	{
		uint16_t animType = *(uint16_t*)anim;
		if ((animType & 0xFF) == 0) {
			animType = SWAP_INT16(animType);
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
			length = SWAP_INT32(length);
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
		boneCount = SWAP_INT16(boneCount);

		uint64_t postBoneDescriptor_offset = (4 + boneCount * sizeof(uint16_t));
		Byte* anim_postBoneDescriptorAddr = (anim + postBoneDescriptor_offset);

		uint64_t animLength = *(uint16_t*)&anim_postBoneDescriptorAddr[0];
		uint64_t __unknown__ = *(uint16_t*)&anim_postBoneDescriptorAddr[4];
		animLength = SWAP_INT16(animLength);
		__unknown__ = SWAP_INT16(__unknown__);

		uint64_t vv73 = 2 * ((4 * __unknown__ + 6) / 2);
		uint64_t aa4 = 6 * (__unknown__ + boneCount);

		Byte* animPtr = anim_postBoneDescriptorAddr + vv73 + aa4;

		unsigned int baseFrame = (unsigned int)animLength - (animLength >= 2 ? 2 : 1);
		unsigned int keyframe = baseFrame / 16;
		unsigned int _v56_intPtr = *(unsigned int*)&animPtr[4 * (uint64_t)keyframe];
		_v56_intPtr = SWAP_INT32(_v56_intPtr);

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

	void convertPtrsToOffsets(void* listAddr, const std::map<gameAddr, uint64_t>& m, uint64_t struct_size, uint64_t amount)
	{
		uint64_t listCursor = (uint64_t)listAddr;
		while (amount-- > 0)
		{
			uint64_t* structMember = (gameAddr*)listCursor;
			gameAddr addr = (gameAddr)*structMember;
			if (m.find(addr) != m.end()) {
				*structMember = m.at(addr);
			}
			else {
				*structMember -= (uint64_t)-1;
			}
			listCursor += struct_size;
		}
	}

	bool endsWith(const std::string& str, const std::string& suffix)
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

	bool startsWith(const std::string& str, const std::string& prefix)
	{
		if (str.length() < prefix.length())
			return false;

		for (size_t i = 0; prefix[i]; ++i)
		{
			if (prefix[i] != str[i]) {
				return false;
			}
		}

		return true;
	}

	std::string getMovesetNameFromFilename(const std::string& filename)
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
