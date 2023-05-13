#pragma once

#include <string>

#include "GameTypes.h"
#include "MovesetStructs.h"

namespace CompressionUtils
{
	namespace FILE {
		namespace Moveset {
			bool Compress(const std::wstring& dest_filename, const std::wstring& src_filename, TKMovesetCompressionType_ compressionType = TKMovesetCompressonType_LZMA);
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
