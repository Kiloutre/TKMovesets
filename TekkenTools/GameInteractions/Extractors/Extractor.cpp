#include <cstdlib>

#include "Extractor.hpp"
#include "GameAddresses.h"

void* Extractor::allocateAndReadBlock(gameAddr blockStart, gameAddr blockEnd, int64_t& size_out)
{
	size_out = blockEnd - blockStart;
	void* block = malloc(sizeof(char) * size_out);
	m_process->readBytes(blockStart, block, size_out);
	// Todo: read by chunks? Doesn't seem needed
	return block;
}