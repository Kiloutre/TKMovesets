#pragma once

# define GAME_ADDR_NULL (0)

// Used for ptr in movesets that can be set to 0
# define MOVESET_ADDR_NULL (-1)

// Is a uint64 so that we can add two addresses together such as module + addr or addr + offset
typedef uint64_t gameAddr;
typedef uint32_t gameAddr32;