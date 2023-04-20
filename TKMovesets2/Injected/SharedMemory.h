#pragma once
#include <cstdint>

struct SharedMemoryPlayer
{
	// Can be null, in which case no moveset is to be loaded
	uint64_t custom_moveset_addr;
};

struct SharedMemory
{
	// Only load custom movesets if locked in
	bool locked_in;
	SharedMemoryPlayer player[4];
};