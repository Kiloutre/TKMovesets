#pragma once

#include <stdint.h>

#include "debug.hpp"

# define GAME_ADDR_NULL (0)

// Is a uint64 so that we can add two addresses together such as module + addr or addr + offset
typedef uint64_t gameAddr;
typedef uint32_t gameAddr32;

// Used for ptr or offsets in movesets that can be set to 0
# define MOVESET_ADDR_MISSING ((gameAddr)-1)
# define MOVESET_ADDR32_MISSING ((gameAddr32)-1)

// Used for ptr to Byte*, i really don't like using char* for those, char* should be explicity for characters
typedef unsigned char Byte;

// IDA sometimes shows us this __int128 type, so define it here
typedef char __ida_int128[16]; // todo: turn this into a struct but without crashing the game when using hooks

#define SET_INT128(s, first, second) ((*(int64_t*)s = first), ((*(int64_t*)((char*)s + 8)) = second))

static inline void print_int128(__ida_int128 val) // todo: remove this
{
#ifdef BUILD_TYPE_DEBUG
	DEBUG_LOG("%llx|%llx, ", *(uint64_t*)&val, *(uint64_t*)(&val + 8));
#endif
}


