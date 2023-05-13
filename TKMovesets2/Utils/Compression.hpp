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
	// This sets the default compression setting across the software
	unsigned int GetDefaultCompressionSetting();
	// Get the index of a compression type
	unsigned int GetCompressionSettingIndex(TKMovesetCompressionType_ compressionType);

	namespace FILE {
		namespace Moveset {
			bool Compress(const std::wstring& dest_filename, const std::wstring& src_filename, TKMovesetCompressionType_ compressionType);
		};
	};

	namespace RAW {
		namespace Moveset {
			Byte* Decompress(Byte* moveset, int32_t compressed_size, uint64_t& size_out);
		};

		namespace LZMA {
			Byte* Compress(Byte* decompressed_data, int32_t decompressed_size, int32_t& size_out, uint8_t preset=0);
			Byte* Decompress(Byte* compressed_data, int32_t compressed_size, int32_t decompressed_size);
		};

		namespace LZ4 {
			Byte* Compress(Byte* decompressed_data, int32_t decompressed_size, int32_t& size_out);
			Byte* Decompress(Byte* compressed_data, int32_t compressed_size, int32_t decompressed_size);
		};
	};
};
