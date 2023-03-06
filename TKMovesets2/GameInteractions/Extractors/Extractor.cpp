#include <format>
#include <fstream>
#include <cstdlib>
#include <windows.h>

#include "Helpers.hpp"
#include "Extractor.hpp"

#include "GameAddresses.h"

// -- helpers -- //

namespace ExtractorUtils
{
	uint64_t getC8AnimSize(GameProcess* process, gameAddr anim)
	{
		uint8_t bone_count = process->readInt8(anim + 2);
		uint32_t header_size = bone_count * 0x4 + 0x8;
		uint32_t frame_size = bone_count * 0x4 * 3;
		uint32_t length = process->readInt32(anim + 4);
		return (int64_t)header_size + (int64_t)frame_size * (int64_t)length;
	}
};

// Private methods //

byte* Extractor::allocateAndReadBlock(gameAddr blockStart, gameAddr blockEnd, uint64_t& size_out)
{
	size_t blockSize = blockEnd - blockStart;

	byte* block = (byte*)malloc(blockSize);
	if (block == nullptr) {
		size_out = 0;
		return nullptr;
	}

	m_process->readBytes(blockStart, block, blockSize);
	size_out = blockSize;

	return block;
}

std::string Extractor::GenerateFilename(const char* characterName, const char* gameIdentifierString, unsigned int suffixId)
{
	if (suffixId <= 1) {
		return std::format("{}/{}{}" MOVESET_FILENAME_EXTENSION, cm_extractionDir, gameIdentifierString, characterName);
	}
	else {
		return std::format("{}/{}{} ({})" MOVESET_FILENAME_EXTENSION, cm_extractionDir, gameIdentifierString, characterName, suffixId);
	}
}

std::string Extractor::GetFilepath(const char* characterName, bool overwriteSameFilename)
{
	CreateDirectory(cm_extractionDir, NULL);
	// Make sure the directory for extraction exists 

	const char* gameIdentifierstring = GetGameIdentifierString();

	std::string filePath = GenerateFilename(characterName, gameIdentifierstring, 0);

	if (!overwriteSameFilename && Helpers::fileExists(filePath.c_str())) {
		filePath = GenerateFilename(characterName, gameIdentifierstring, 1);
		unsigned int counter = 1;
		while (Helpers::fileExists(filePath.c_str())) {
			counter++;
			filePath = GenerateFilename(characterName, gameIdentifierstring, counter);
		}
	}

	return filePath;
}
