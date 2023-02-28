#include <cstdlib>

#include "Extractor.hpp"
#include "GameAddresses.h"

# define CHUNK_SIZE 4096

void* Extractor::allocateAndReadBlock(gameAddr blockStart, gameAddr blockEnd, uint64_t& size_out)
{
	uint64_t blockSize = blockEnd - blockStart;
	void* block = malloc(sizeof(char) * blockSize);
	size_out = blockSize;

	uint64_t readCursor = (uint64_t)blockStart;
	uint64_t writeCursor = (uint64_t)block;

	m_process->readBytes((gameAddr)readCursor, (void*)writeCursor, blockSize);

	// Todo: read by chunks? Doesn't seem needed
	return block;
}
