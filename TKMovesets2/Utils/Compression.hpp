#pragma once

#include <string>

#include "ExtractorSettings.hpp"

#include "GameTypes.h"
#include "MovesetStructs.h"

struct s_compressionTypes {
	const char* name;
	TKMovesetCompressionType_ compressionSetting;
	ExtractSettings_ extractSetting;
};

namespace CompressionUtils
{
	// Returns a compression setting & its name
	const s_compressionTypes& GetCompressionSetting(uint8_t idx);
	// Returns the list of useable compressions
	size_t GetCompressionSettingCount();
	// Returns the default compression type
	TKMovesetCompressionType GetDefaultCompression();
	// This sets the default compression setting across the software
	unsigned int GetDefaultCompressionSetting();
	// Get the index of a compression type
	unsigned int GetCompressionSettingIndex(TKMovesetCompressionType_ compressionType);

	namespace FILE {
		namespace Moveset {
			// Compress a moveset in place
			bool Compress(const std::wstring& dest_filename, TKMovesetCompressionType_ compressionType);
			// Compress a moveset from file to file
			bool Compress(const std::wstring& dest_filename, const std::wstring& src_filename, TKMovesetCompressionType_ compressionType);
		};
	};

	namespace RAW {
		// Tools to decompress moveset specifically, will adapt to the moveset's compression algorithm indicated in the header
		namespace Moveset {
			// Decompress moveset (keeps header in the final allocated memory area)
			Byte* DecompressWithHeader(const Byte* moveset, uint64_t compressed_data_size, uint64_t& size_out);
			// Decompress moveset (without keeping header)
			Byte* Decompress(const Byte* moveset, uint64_t compressed_data_size, uint64_t& size_out);
			// Decompress a moveset (without keeping header) to a previously allocated buffer
			bool DecompressToBuffer(const Byte* moveset, uint64_t compressed_data_size, Byte* output_buffer);
			
			// Compress the moveset from memory to memory
			Byte* Compress(const Byte* moveset, uint64_t full_size, TKMovesetCompressionType_ compressionType, uint64_t& size_out);
		};

		// Lzma, slow but very good compression (about 50% for T movesets, at preset 0, which is the default)
		// Preset can be between 0 and 9 included. Higher number compresses more
		// But the ratio is not that much better for higher preset, and speed & ram usage is SIGNIFICANTLY worse
		namespace LZMA {
			// Compress without allocation
			uint64_t CompressToBuffer(const Byte* input_data, uint64_t input_size, Byte* output_buffer, uint64_t output_bufsize, uint8_t preset=0);
			// Compress with allocation
			Byte* Compress(const Byte* input_data, uint64_t input_size, uint64_t& size_out, uint8_t preset = 0);

			// Decompress without allocation
			bool DecompressToBuffer(const Byte* compressed_data, uint64_t compressed_size, Byte* output_buffer, uint64_t decompressed_size);
			// Decompress with allocation
			Byte* Decompress(const Byte* compressed_data, uint64_t compressed_size, uint64_t decompressed_size);
		};

		// LZ4, very quick with viable compression (about 33% for T movesets)
		namespace LZ4 {
			// Compress without allocation
			uint64_t CompressToBuffer(const Byte* input_data, uint64_t input_size, Byte* output_buffer, uint64_t output_bufsize);
			// Compress with allocation
			Byte* Compress(const Byte* input_data, uint64_t input_size, uint64_t& size_out);

			// Decompress without allocation
			bool DecompressToBuffer(const Byte* compressed_data, uint64_t compressed_size, Byte* output_buffer, uint64_t decompressed_size);
			// Decompress with allocation
			Byte* Decompress(const Byte* compressed_data, uint64_t compressed_size, uint64_t decompressed_size);
		};
	};
};
