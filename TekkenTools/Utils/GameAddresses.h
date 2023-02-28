#pragma once

# define GAME_ADDR_NULL ((gameAddr)0)

// Is a int64 so that we can add two addresses together such as module + addr or addr + offset
typedef int64_t gameAddr;