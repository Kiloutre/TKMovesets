#include <format>
#include <fstream>
#include <cstdlib>
#include <windows.h>

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
		return header_size + frame_size * length;
	}
};

void* Extractor::allocateAndReadBlock(gameAddr blockStart, gameAddr blockEnd, uint64_t& size_out)
{
	size_t blockSize = blockEnd - blockStart;

	void* block = malloc(sizeof(char) * blockSize);
	m_process->readBytes(blockStart, block, blockSize);
	size_out = blockSize;

	return block;
}

bool Extractor::CreateMovesetFile(const char* characterName, const char* gameIdentifierstring)
{
	CreateDirectory(cm_extractionDir, NULL);

	std::string filePath = std::format("{}/{}{}" MOVESET_FILENAME_EXTENSION, cm_extractionDir, gameIdentifierstring, characterName);
	m_file.open(filePath.c_str(), std::ios::binary | std::ios::out);
	return !m_file.fail();
}

void Extractor::CloseMovesetFile()
{
	m_file.close();
}