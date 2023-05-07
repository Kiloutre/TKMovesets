#include <lz4.h>
#include <filesystem>
#include <fstream>

#include "Compression.hpp"

#include "MovesetStructs.h"

namespace CompressionUtils
{
	bool DecompressMoveset(Byte* outputDest, const Byte* moveset_data_start, uint64_t src_size, int32_t original_size)
	{
		int32_t decompressedSize = (int)LZ4_decompress_safe((char*)moveset_data_start, (char*)outputDest, (int)src_size, (int)original_size);

		if (decompressedSize <= 0) {
			DEBUG_LOG("Error during decompression: original_size is %d, srcsize is %llu, decompressed size is %d\n", original_size, src_size, decompressedSize);
			return false;
		}

		return true;
	}

	bool CompressFile(int32_t moveset_data_start, const std::wstring& dest_filename, const std::wstring& src_filename)
	{
		std::ifstream orig_file(src_filename, std::ios::binary);
		std::ofstream new_file(dest_filename, std::ios::binary);

		// Copy up to moveset_data_start, get size of moveset data
		int32_t moveset_data_size;
		{
			// Read up to moveset data start
			char* buf = new char[moveset_data_start];
			orig_file.read(buf, moveset_data_start);

			// Calculate moveset data size
			orig_file.seekg(0, std::ios::end);
			moveset_data_size = (int32_t)orig_file.tellg() - moveset_data_start;

			// Mark moveset as compressed
			((TKMovesetHeader*)buf)->compressionType = TKMovesetCompressonType_LZ4;
			((TKMovesetHeader*)buf)->moveset_data_size = moveset_data_size;

			// Write header
			new_file.write(buf, moveset_data_start);
			delete[] buf;

			// Move cursor back to the start of the moveset data
			orig_file.seekg(moveset_data_start, std::ios::beg);
		}

		// Compress moveset data
		char* inbuf = new char[moveset_data_size];
		char* outbuf = new char[moveset_data_size];
		int32_t compressed_size;

		orig_file.read(inbuf, moveset_data_size);
		orig_file.close();

		std::filesystem::remove(src_filename);

		compressed_size = LZ4_compress_default(inbuf, outbuf, moveset_data_size, moveset_data_size);
		DEBUG_LOG("Compression: Old size was %d, compressed size is %d\n", moveset_data_size, compressed_size);

		if (compressed_size <= 0)
		{
			new_file.close();
			std::filesystem::remove(dest_filename);
			DEBUG_LOG("Compression failure\n");
		}
		else {
			new_file.write(outbuf, compressed_size);
		}

		delete[] inbuf;
		delete[] outbuf;
		return compressed_size >= 0;
	}
}