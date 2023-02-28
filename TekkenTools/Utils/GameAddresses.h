#pragma once

# define GAME_ADDR_NULL (0)

// Is a int64 so that we can add two addresses together such as module + addr or addr + offset
typedef uint64_t gameAddr;
typedef uint32_t gameAddr32;