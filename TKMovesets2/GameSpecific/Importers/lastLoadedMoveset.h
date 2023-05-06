#ifndef LAST_LOADED_DEFINED
# define LAST_LOADED_DEFINED
struct s_lastLoaded {
	// Last loaded moveset's CRC32 ID, used right after import by online importer
	uint8_t charId = 0;
	// Last loaded moveset's character ID, used right after import by online importer
	uint32_t crc32 = 0;
	// Stores the in-game address of the moveset successfully loaded by the last Import() call
	gameAddr address = 0;
	// Size of the uncompressed moveset, from moveset_data_start
	uint64_t size = 0;
	// Stores the in-game address of the moveset before any modification has been made to it (which means it might still be compressed)
	gameAddr originalDataAddress = 0;
	// Size of the original moveset data
	gameAddr originalDataSize = 0;
};
#endif