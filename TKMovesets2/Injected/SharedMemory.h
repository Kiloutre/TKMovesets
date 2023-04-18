#pragma once
#include <cstdint>

struct SharedMemoryPlayer
{
	// Can be null, in which case no moveset is to be loaded
	uint64_t custom_moveset_addr;
};

struct SharedMemory
{
	SharedMemoryPlayer player[4];
};