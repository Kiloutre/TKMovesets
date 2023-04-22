#include <format>
#include <fstream>
#include <cstdlib>
#include <windows.h>
#include <filesystem>
//#include "lzma.h"

#include "Helpers.hpp"
#include "Extractor.hpp"

#include "GameTypes.h"
#include "lz4.h"

// -- Helpers -- //

namespace ExtractorUtils
{
	uint64_t getC8AnimSize(GameProcess* process, gameAddr anim)
	{
		bool isSwapped = process->readInt8(anim + 0) == 0;

		uint8_t bone_count = process->readInt8(anim + (isSwapped ? 3 : 2));
		uint32_t header_size = bone_count * 0x4 + 0x8;
		uint32_t frame_size = bone_count * 0x4 * 3;

		uint32_t length = process->readInt32(anim + 4);
		if (isSwapped) {
			length = SWAP_INT32(length);
		}

		return (int64_t)header_size + (int64_t)frame_size * (int64_t)length;
	}

	uint64_t get64AnimSize(GameProcess* process, gameAddr anim)
	{
		bool isSwapped = process->readInt8(anim + 0) == 0;
		return isSwapped ? get64AnimSize_BigEndian(process,anim) : get64AnimSize_LittleEndian(process, anim);
	}

	uint64_t get64AnimSize_BigEndian(GameProcess* process, gameAddr anim)
	{
		// Do all calculations in uint64_t that way i don't have to pay attention to possible overflows

		uint64_t boneCount = process->readInt16(anim + 2);
		boneCount = SWAP_INT16(boneCount);

		uint64_t postBoneDescriptor_offset = (4 + boneCount * sizeof(uint16_t));
		gameAddr anim_postBoneDescriptorAddr = (gameAddr)(anim + postBoneDescriptor_offset);

		uint64_t animLength = (uint16_t)process->readInt16(anim_postBoneDescriptorAddr);
		uint64_t __unknown__ = (uint16_t)process->readInt16(anim_postBoneDescriptorAddr + 4);
		animLength = SWAP_INT16(animLength);
		__unknown__ = SWAP_INT16(__unknown__);

		uint64_t vv73 = 2 * ((4 * __unknown__ + 6) / 2);
		uint64_t aa4 = 6 * (__unknown__ + boneCount);

		gameAddr animPtr = anim_postBoneDescriptorAddr + vv73 + aa4;

		unsigned int baseFrame = (unsigned int)animLength - (animLength >= 2 ? 2 : 1);
		unsigned int keyframe = baseFrame / 16;
		unsigned int _v56_intPtr = (unsigned int)process->readInt32(animPtr + 4 * (uint64_t)keyframe);
		_v56_intPtr = SWAP_INT32(_v56_intPtr);

		gameAddr animPtr_2 = animPtr + _v56_intPtr;
		int lastArg_copy = (int)boneCount;

		do
		{
			for (int i = 0; i < 3; ++i)
			{
				Byte v58 = process->readInt8(animPtr_2);
				int offsetStep = v58 / 4;
				animPtr_2 += offsetStep;
			}
		} while (--lastArg_copy != 0);

		return (uint64_t)animPtr_2 - (uint64_t)anim;
	}

	uint64_t get64AnimSize_LittleEndian(GameProcess* process, gameAddr anim)
	{
		// Do all calculations in uint64_t that way i don't have to pay attention to possible overflows

		uint64_t boneCount = process->readInt16(anim + 2);

		uint64_t postBoneDescriptor_offset = (4 + boneCount * sizeof(uint16_t));
		gameAddr anim_postBoneDescriptorAddr = (gameAddr)(anim + postBoneDescriptor_offset);

		uint64_t animLength = (uint16_t)process->readInt16(anim_postBoneDescriptorAddr);
		uint64_t __unknown__ = (uint16_t)process->readInt16(anim_postBoneDescriptorAddr + 4);

		uint64_t vv73 = 2 * ((4 * __unknown__ + 6) / 2);
		uint64_t aa4 = 6 * (__unknown__ + boneCount);

		gameAddr animPtr = anim_postBoneDescriptorAddr + vv73 + aa4;

		unsigned int baseFrame = (unsigned int)animLength - (animLength >= 2 ? 2 : 1);
		unsigned int keyframe = baseFrame / 16;
		unsigned int _v56_intPtr = (unsigned int)process->readInt32(animPtr + 4 * (uint64_t)keyframe);

		gameAddr animPtr_2 = animPtr + _v56_intPtr;
		int lastArg_copy = (int)boneCount;

		do
		{
			for (int i = 0; i < 3; ++i)
			{
				Byte v58 = process->readInt8(animPtr_2);
				int offsetStep = v58 / 4;
				animPtr_2 += offsetStep;
			}
		} while (--lastArg_copy != 0);

		return (uint64_t)animPtr_2 - (uint64_t)anim;
	}

	void WriteFileData(std::ofstream &file, const std::vector<std::pair<Byte*, uint64_t>>& blocks, uint8_t&progress, uint8_t progress_max)
	{
		uint8_t remainingProgress = progress_max - progress;
		uint8_t step = (uint8_t)((size_t)remainingProgress / blocks.size());

		for (const std::pair<Byte*, uint64_t>& block : blocks) {
			char* blockData = (char*)block.first;
			uint64_t blockSize = block.second;

			file.write(blockData, blockSize);
			Helpers::align8Bytes(file);

			progress += step;
		}
	}

	void CompressFile(const std::wstring& dest_filename, const std::wstring& src_filename)
	{
		std::filesystem::rename(src_filename.c_str(), dest_filename.c_str());
	}
};

// Private methods //

Byte* Extractor::allocateAndReadBlock(gameAddr blockStart, gameAddr blockEnd, uint64_t& size_out)
{
	size_t blockSize = blockEnd - blockStart;

	if (blockSize >= (MOVESET_MAX_BLOCK_SIZE_MB * 1000000)) { // 50 MB
		DEBUG_LOG("Block size too big to be valid, returning nullptr\n");
		// Arbitrary max block size in case something bad happened and we get a very invalid start & end
		return nullptr;
	}

	Byte* block = (Byte*)malloc(blockSize);
	if (block == nullptr) {
		size_out = 0;
		return nullptr;
	}

	m_process->readBytes(blockStart, block, blockSize);
	size_out = blockSize;

	return block;
}

std::wstring Extractor::GenerateFilename(const char* characterName, const char* gameIdentifierString, const char* extension, unsigned int suffixId)
{
	if (suffixId <= 1) {
		std::wstring outputFilename = Helpers::string_to_wstring(cm_extractionDir) + L"/" +
			Helpers::string_to_wstring(gameIdentifierString) + Helpers::string_to_wstring(characterName)
			+ Helpers::string_to_wstring(extension);
		return outputFilename;
	}
	else {
		std::wstring outputFilename = Helpers::string_to_wstring(cm_extractionDir) + L"/" +
			Helpers::string_to_wstring(gameIdentifierString) + Helpers::string_to_wstring(characterName)
			+ L" (" + std::to_wstring(suffixId) + L")" + Helpers::string_to_wstring(extension);
		return outputFilename;
	}
}

void Extractor::GetFilepath(const char* characterName, std::wstring& out, std::wstring& out_tmp, bool overwriteSameFilename)
{
	CreateDirectory(cm_extractionDir, NULL);
	// Make sure the directory for extraction exists 

	const char* gameIdentifierstring = GetGameIdentifierString();

	std::wstring filePath = GenerateFilename(characterName, gameIdentifierstring, MOVESET_FILENAME_EXTENSION, 0);

	if (!overwriteSameFilename && Helpers::fileExists(filePath.c_str())) {
		// File exists: try to add numbered suffix, loop until we find a free one
		unsigned int counter = 2;
		filePath = GenerateFilename(characterName, gameIdentifierstring, MOVESET_FILENAME_EXTENSION, counter);

		while (Helpers::fileExists(filePath.c_str())) {
			filePath = GenerateFilename(characterName, gameIdentifierstring, MOVESET_FILENAME_EXTENSION, counter);
			counter++;
		}

		out = filePath;
		out_tmp = GenerateFilename(characterName, gameIdentifierstring, MOVESET_TMPFILENAME_EXTENSION, counter);
	}
	else {
		// File doesnt exist or we are allowed to overwrite, no need to suffix
		out = filePath;
		out_tmp = GenerateFilename(characterName, gameIdentifierstring, MOVESET_TMPFILENAME_EXTENSION);
	}
}

uint64_t Extractor::GetAnimationSize(gameAddr anim)
{
	uint16_t animType = m_process->readInt16(anim);
	if ((animType & 0xFF) == 0) {
		animType = SWAP_INT16(animType);
	}

	if (animType == 0xC8) {
		return ExtractorUtils::getC8AnimSize(m_process, anim);
	}
	else if (animType == 0x64) {
		return ExtractorUtils::get64AnimSize(m_process, anim);
	}
	// Invalid animation type

	DEBUG_LOG("Animation address %llx does not have a valid animation type. First four bytes: %x\n", anim, m_process->readInt32(anim));

	throw;
	return 0;
}