#include <format>
#include <fstream>
#include <cstdlib>
#include <windows.h>

#include "Extractor.hpp"

#include "GameAddresses.h"

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