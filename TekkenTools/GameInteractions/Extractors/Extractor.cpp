#include <format>
#include <fstream>
#include <cstdlib>

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

void Extractor::CreateMovesetFile(const char* characterName)
{
	std::string filePath = std::format("{}/{}.txt", extractionDir, characterName);
	m_file.open(filePath.c_str(), std::ios::binary | std::ios::out);
}

void Extractor::CloseMovesetFile()
{
	m_file.close();
}