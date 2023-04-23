#pragma once

# include <cstdint>
# include <windef.h>

struct SharedMemBase
{
	// Only load custom movesets if locked in
	bool locked_in;
	//wchar_t game_addresses_path[MAX_PATH];
};