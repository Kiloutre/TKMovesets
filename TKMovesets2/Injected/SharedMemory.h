#pragma once
#include <cstdint>

struct SharedMemoryPlayer
{
	// Can be null, in which case no moveset is to be loaded
	uint64_t customMovesetAddr;
};

struct SharedMemory
{
	SharedMemoryPlayer player[4];
};