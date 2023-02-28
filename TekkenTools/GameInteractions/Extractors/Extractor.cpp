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
