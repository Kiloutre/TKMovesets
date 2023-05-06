#pragma once

#include <string>

#include "GameTypes.h"

namespace CompressionUtils
{
	// Return an allocated copy of the decompressed moveset data (moveset_data_start and on)
	bool DecompressMoveset(Byte* outputDest, const Byte* moveset_data_start, uint64_t src_size, int32_t original_size);

	// Compress a moveset. Returns false on failure. Modifies header->moveset_data_size automatically.
	bool CompressFile(int32_t moveset_data_start, const std::wstring& dest_filename, const std::wstring& src_filename);
};
