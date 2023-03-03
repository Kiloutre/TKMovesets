#include <format>
#include <fstream>
#include <cstdlib>
#include <windows.h>

#include "Helpers.hpp"
#include "Extractor.hpp"

#include "GameAddresses.h"

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

char* Extractor::allocateAndReadBlock(gameAddr blockStart, gameAddr blockEnd, uint64_t& size_out)
{
	size_t blockSize = blockEnd - blockStart;

	char* block = (char*)malloc(blockSize);
	if (block == nullptr) {
		size_out = 0;
		return nullptr;
	}

	m_process->readBytes(blockStart, block, blockSize);
	size_out = blockSize;

	return block;
}

std::string Extractor::GetFilename(const char* characterName, const char* gameIdentifierString, unsigned int suffixId)
{
	if (suffixId <= 1) {
		return std::format("{}/{}{}" MOVESET_FILENAME_EXTENSION, cm_extractionDir, gameIdentifierString, characterName);
	}
	else {
		return std::format("{}/{}{} ({})" MOVESET_FILENAME_EXTENSION, cm_extractionDir, gameIdentifierString, characterName, suffixId);
	}
}

bool Extractor::CreateMovesetFile(const char* characterName, const char* gameIdentifierstring, bool overwriteSameFilename)
{
	CreateDirectory(cm_extractionDir, NULL);

	std::string filePath = GetFilename(characterName, gameIdentifierstring);

	if (!overwriteSameFilename && Helpers::fileExists(filePath.c_str())) {
		filePath = GetFilename(characterName, gameIdentifierstring, 1);
		unsigned int counter = 1;
		while (Helpers::fileExists(filePath.c_str())) {
			counter++;
			filePath = GetFilename(characterName, gameIdentifierstring, counter);
		}
	}

	m_file.open(filePath.c_str(), std::ios::binary | std::ios::out);
	return !m_file.fail();
}

void Extractor::CloseMovesetFile()
{
	m_file.close();
}