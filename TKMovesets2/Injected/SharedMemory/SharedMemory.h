#pragma once

# include <cstdint>
# include <windef.h>

struct SharedMemBase
{
	// Only load custom movesets if locked in
	bool locked_in;
	// Contains the version of the moveset loader
	char moveset_loader_version[32];
	//wchar_t game_addresses_path[MAX_PATH];
};