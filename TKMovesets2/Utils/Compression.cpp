#include "FileIO.hpp"
#include <stdexcept>
#include <lz4.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
#include <atomic>
#include <windows.h>
#include <lzma.h>

#include "Compression.hpp"
#include "CompactZstd.hpp"

#include "MovesetStructs.h"
#include "MovesetLimits.hpp"

// -- Available compression settings across the software -- //

const s_compressionTypes g_compressionTypes[] = {
	{
		.name = "", // None. Should be the first element in the array, always.
		.compressionSetting = TKMovesetCompressionType_None,
		.extractSetting = (ExtractSettings_)0
	},

	{
		.name = "LZMA",
		.compressionSetting = TKMovesetCompressionType_LZMA,
		.extractSetting = ExtractSettings_CompressLZMA
	},

	{
		.name = "LZ4",
		.compressionSetting = TKMovesetCompressionType_LZ4,
		.extractSetting = ExtractSettings_CompressLZ4
	},
	{
		.name = "Compact + Zstd",
		.compressionSetting = TKMovesetCompressionType_CompactZstd,
		.extractSetting = ExtractSettings_CompressCompactZstd
	}
};
const size_t g_compressionTypes_len = _countof(g_compressionTypes);

namespace CompressionUtils
{
	const s_compressionTypes& GetCompressionSetting(uint8_t idx)
	{
		if (idx >= g_compressionTypes_len) {
			DEBUG_LOG("Bad compression setting: %u\n", idx);
			throw std::out_of_range("Invalid compression setting");
		}

		return g_compressionTypes[idx];
	}

	size_t GetCompressionSettingCount()
	{
		return g_compressionTypes_len;
	}

	TKMovesetCompressionType GetDefaultCompression()
	{
#ifdef BUILD_TYPE_DEBUG
		return TKMovesetCompressionType_None;
#else
		return TKMovesetCompressionType_LZ4;
#endif
	}

	unsigned int GetDefaultCompressionSetting()
	{
		auto defaultCompression = GetDefaultCompression();
		for (unsigned int i = 0; i < g_compressionTypes_len; ++i) {
			if (g_compressionTypes[i].compressionSetting == defaultCompression) {
				return i;
			}
		}
		return 0;
	}

	unsigned int GetCompressionSettingIndex(TKMovesetCompressionType_ compressionType)
	{
		for (unsigned int i = 0; i < g_compressionTypes_len; ++i) {
			if (g_compressionTypes[i].compressionSetting == compressionType) {
				return i;
			}
		}
		return 0;
	}

	namespace FILE
	{
		namespace Moveset
		{
			bool Compress(const std::wstring& filename, TKMovesetCompressionType_ compressionType)
			{
				return Compress(filename, filename, compressionType);
			}

			bool Compress(const std::wstring& dest_filename, const std::wstring& src_filename, TKMovesetCompressionType_ compressionType)
			{
				try {
                    std::ifstream source(src_filename, std::ios::binary | std::ios::ate);
                    auto length = source.tellg();
                    if (length < static_cast<std::streamoff>(sizeof(TKMovesetHeader)) || length > static_cast<std::streamoff>(MovesetLimits::MaximumStored)) return false;
                    TKMovesetHeader prefix;
                    source.seekg(0);
                    if (!source.read(reinterpret_cast<char*>(&prefix), sizeof(prefix)) || !prefix.ValidateHeader(length) || !MovesetLimits::Fits(prefix, length)) return false;
                    std::vector<Byte> bytes(static_cast<size_t>(length));
                    source.seekg(0);
                    if (!source.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) return false;
                    source.close();

					uint64_t size = 0;
					std::unique_ptr<Byte[]> compressed(RAW::Moveset::Compress(bytes.data(), bytes.size(), compressionType, size));
					if (!compressed) return false;

                    if (!FileIO::WriteAtomically(dest_filename, std::span(compressed.get(), static_cast<size_t>(size)))) return false;
                    std::error_code error;
					if (src_filename != dest_filename && !std::filesystem::equivalent(src_filename, dest_filename, error)) {
						std::filesystem::remove(src_filename, error);
					}
					return true;
				} catch (const std::exception&) {
					return false;
				}
			}
		};
	};

	namespace RAW
	{
		namespace Moveset
		{
			Byte* Decompress(const Byte* moveset, uint64_t compressed_data_size, uint64_t& size_out)
			{
				TKMovesetHeader* header = (TKMovesetHeader*)moveset;

				if (!header->isCompressed()) {
					return nullptr;
				}

				const Byte* moveset_data_start = moveset + header->moveset_data_start;
				size_out = header->moveset_data_size;
				Byte* result = nullptr;

				switch (header->compressionType)
				{
				case TKMovesetCompressionType_LZ4:
					result = CompressionUtils::RAW::LZ4::Decompress(moveset_data_start, compressed_data_size, header->moveset_data_size);
					break;
				case TKMovesetCompressionType_LZMA:
					result = CompressionUtils::RAW::LZMA::Decompress(moveset_data_start, compressed_data_size, header->moveset_data_size);
					break;
				case TKMovesetCompressionType_CompactZstd: {
                    if (size_out > CompactZstd::MaximumPayload) return nullptr;
                    std::unique_ptr<Byte[]> output(new (std::nothrow) Byte[size_out]);
                    if (output && CompactZstd::Decompress({moveset_data_start, static_cast<size_t>(compressed_data_size)}, {output.get(), static_cast<size_t>(size_out)})) result = output.release();
                    break;
                }
				default:
					size_out = 0;
					DEBUG_ERR("Moveset decompression: unhandled compression type '%u'", header->compressionType);
					return nullptr;
					break;
				}

				if (result == nullptr) {
					DEBUG_ERR("Error during decompression");
				}

				DEBUG_LOG("Decompressed moveset, compressed size was %llu, size_out is %llu\n", compressed_data_size, size_out);

				return result;
			}

			bool DecompressToBuffer(const Byte* moveset, uint64_t compressed_data_size, Byte* output_buffer)
			{
				TKMovesetHeader* header = (TKMovesetHeader*)moveset;

				if (!header->isCompressed()) {
					return false;
				}

				const Byte* moveset_data_start = moveset + header->moveset_data_start;
				bool result = false;

				switch (header->compressionType)
				{
				case TKMovesetCompressionType_LZ4:
					result = CompressionUtils::RAW::LZ4::DecompressToBuffer(moveset_data_start, compressed_data_size, output_buffer, header->moveset_data_size);
					break;
				case TKMovesetCompressionType_LZMA:
					result = CompressionUtils::RAW::LZMA::DecompressToBuffer(moveset_data_start, compressed_data_size, output_buffer, header->moveset_data_size);
					break;
				case TKMovesetCompressionType_CompactZstd:
                    result = CompactZstd::Decompress({moveset_data_start, static_cast<size_t>(compressed_data_size)}, {output_buffer, static_cast<size_t>(header->moveset_data_size)});
                    break;
				default:
					DEBUG_ERR("Moveset decompression: unhandled compression type '%u'", header->compressionType);
					return false;
					break;
				}

				if (result == false) {
					DEBUG_ERR("Error during decompression");
				}

				DEBUG_LOG("Decompressed moveset, compressed size was %llu, result was %llu\n", compressed_data_size, header->moveset_data_size);

				return result;
			}

			bool ExpandFileBuffer(Byte*& moveset, uint64_t& fileSize)
			{
				if (!moveset || fileSize < sizeof(TKMovesetHeader)) return false;
				const auto* header = reinterpret_cast<const TKMovesetHeader*>(moveset);
				if (!header->ValidateHeader(fileSize)) return false;
				if (fileSize > MovesetLimits::Stored(*header)) return false;
				if (!header->isCompressed()) return fileSize <= MovesetLimits::Expanded(*header);
				const size_t prefix = header->moveset_data_start;
				const auto limit = MovesetLimits::Expanded(*header);
				if (prefix > limit || header->moveset_data_size > limit - prefix) return false;
				const size_t expandedSize = prefix + header->moveset_data_size;
				// Callers read the whole header struct from the expanded buffer.
				if (expandedSize < sizeof(TKMovesetHeader)) return false;
				auto expanded = static_cast<Byte*>(malloc(expandedSize));
				if (!expanded) return false;
				if (!DecompressToBuffer(moveset, fileSize - prefix, expanded + prefix)) {
					free(expanded);
					return false;
				}
				memcpy(expanded, moveset, prefix);
				auto expandedHeader = reinterpret_cast<TKMovesetHeader*>(expanded);
				expandedHeader->compressionType = TKMovesetCompressionType_None;
				expandedHeader->moveset_data_size = 0;
				free(moveset);
				moveset = expanded;
				fileSize = expandedSize;
				return true;
			}

			Byte* Compress(const Byte* moveset, uint64_t full_size, TKMovesetCompressionType_ compressionType, uint64_t& size_out)
			{
				size_out = 0;
				if (full_size < sizeof(TKMovesetHeader)) {
					return nullptr;
				}

				const TKMovesetHeader* header = (TKMovesetHeader*)moveset;

				if (header->isCompressed()) {
					DEBUG_LOG("Can't compress already compressed movesets.\n");
					return nullptr;
				}

				if (!header->ValidateHeader(full_size) || full_size > MovesetLimits::Expanded(*header)) return nullptr;
                if (compressionType == TKMovesetCompressionType_None) {
                    auto copy = new (std::nothrow) Byte[full_size];
                    if (!copy) return nullptr;
                    memcpy(copy, moveset, full_size);
                    size_out = full_size;
                    return copy;
                }
				const size_t prefix = header->moveset_data_start;
				const size_t payload = full_size - prefix;
				uint64_t encodedSize = 0;
				std::unique_ptr<Byte[]> encoded;
				switch (compressionType) {
				case TKMovesetCompressionType_LZ4:
					encoded.reset(LZ4::Compress(moveset + prefix, payload, encodedSize));
					break;
				case TKMovesetCompressionType_LZMA:
					encoded.reset(LZMA::Compress(moveset + prefix, payload, encodedSize));
					break;
				case TKMovesetCompressionType_CompactZstd:
                    try {
                        const auto compact = CompactZstd::Compress({moveset + prefix, payload});
                        if (compact.size() > MovesetLimits::Stored(*header) - prefix) return nullptr;
                        encoded.reset(new Byte[compact.size()]);
                        memcpy(encoded.get(), compact.data(), compact.size()); encodedSize = compact.size();
                    } catch (const std::exception&) { return nullptr; }
                    break;
				default:
					return nullptr;
				}
				if (!encoded || encodedSize > SIZE_MAX - prefix) return nullptr;
				// Incompressible data can grow (LZ4/LZMA framing): refuse output the loader would reject
				// (ExpandFileBuffer: stored size above MovesetLimits::Stored) instead of saving a file that cannot reopen.
				if (encodedSize > MovesetLimits::Stored(*header) - prefix) return nullptr;
				auto result = new (std::nothrow) Byte[prefix + encodedSize];
				if (!result) return nullptr;
				memcpy(result, moveset, prefix);
				memcpy(result + prefix, encoded.get(), encodedSize);
				auto newHeader = reinterpret_cast<TKMovesetHeader*>(result);
				newHeader->compressionType = compressionType;
				newHeader->moveset_data_size = payload;
				size_out = prefix + encodedSize;
				return result;
			}
		};

		namespace LZMA
		{
			uint64_t CompressToBuffer(const Byte* input_data, uint64_t input_size, Byte* output_buffer, uint64_t output_bufsize, uint8_t preset)
			{
				size_t written = 0;
				auto status = lzma_easy_buffer_encode((std::min)(preset, uint8_t{9}), LZMA_CHECK_CRC64,
					nullptr, input_data, input_size, output_buffer, &written, output_bufsize);
				return status == LZMA_OK ? written : 0;
			}

			bool DecompressToBuffer(const Byte* input, uint64_t size, Byte* output, uint64_t expected)
			{
				uint64_t memoryLimit = 256ULL << 20;
				size_t consumed = 0, written = 0;
				auto status = lzma_stream_buffer_decode(&memoryLimit, LZMA_CONCATENATED, nullptr,
					input, &consumed, size, output, &written, expected);
				return status == LZMA_OK && consumed == size && written == expected;
			}

			Byte* Compress(const Byte* input, uint64_t size, uint64_t& size_out, uint8_t preset)
			{
				size_out = 0;
				size_t capacity = lzma_stream_buffer_bound(size);
				std::unique_ptr<Byte[]> output(new (std::nothrow) Byte[capacity]);
				if (output) size_out = CompressToBuffer(input, size, output.get(), capacity, preset);
				return size_out ? output.release() : nullptr;
			}

			Byte* Decompress(const Byte* input, uint64_t size, uint64_t expected)
			{
				std::unique_ptr<Byte[]> output(new (std::nothrow) Byte[expected]);
				if (!output || !DecompressToBuffer(input, size, output.get(), expected)) return nullptr;
				return output.release();
			}
		}

		namespace LZ4
		{
			uint64_t CompressToBuffer(const Byte* input_data, uint64_t input_size, Byte* output_buffer, uint64_t output_bufsize)
			{
				if (input_size > INT_MAX) {
					// lz4 only supports up to INT_MAX size in both input and output
					return 0;
				}

				if (output_bufsize > INT_MAX) {
					// cap max output size at int max (to avoid overflow when making the function call)
					output_bufsize = INT_MAX;
				}

				int compressed_size = LZ4_compress_default((char*)input_data, (char*)output_buffer, (int)input_size, (int)output_bufsize);
				return compressed_size >= 0 ? compressed_size : 0;
			}

			bool DecompressToBuffer(const Byte* compressed_data, uint64_t compressed_size, Byte* output_buffer, uint64_t decompressed_size)
			{
				if (compressed_size > INT_MAX || decompressed_size > INT_MAX) {
					// lz4 only supports up to INT_MAX size in both input and output
					return false;
				}

				int decompressedSize;
				decompressedSize = LZ4_decompress_safe((char*)compressed_data, (char*)output_buffer, (int)compressed_size, (int)decompressed_size);

				if (decompressedSize < 0 || static_cast<uint64_t>(decompressedSize) != decompressed_size)
				{
					DEBUG_LOG("LZ4 Decompression failure\n");
					return false;
				}

				return true;
			}

			Byte* Compress(const Byte* decompressed_data, uint64_t decompressed_size, uint64_t& size_out)
			{
				size_out = 0;

				if (decompressed_size > LZ4_MAX_INPUT_SIZE) {
					// lz4 only supports up to INT_MAX size
					return nullptr;
				}

				Byte* result;
                
                try {
                    result = new Byte[LZ4_compressBound(static_cast<int>(decompressed_size))];
                } catch (std::bad_alloc&) {
                    DEBUG_ERR("Failed to allocate %llu bytes for decompressed data", decompressed_size);
                    return nullptr;
                }

				int compressed_size = LZ4_compress_default((char*)decompressed_data, (char*)result, (int)decompressed_size, LZ4_compressBound(static_cast<int>(decompressed_size)));

				if (compressed_size <= 0)
				{
					size_out = 0;
					DEBUG_LOG("LZ4 Compression failure\n");
					delete[] result;
					return nullptr;
				}
				else {
					size_out = (unsigned int)compressed_size;
				}

				return result;
			}

			Byte* Decompress(const Byte* compressed_data, uint64_t compressed_size, uint64_t decompressed_size)
			{
				if (decompressed_size > INT_MAX || compressed_size > INT_MAX) {
					// lz4 only supports up to INT_MAX size
					return nullptr;
				}

				Byte* result;
                try {
                    result = new Byte[decompressed_size];
                } catch (std::bad_alloc&) {
                    DEBUG_ERR("Failed to allocate %llu bytes for decompressed data", decompressed_size);
                    return nullptr;
                }

				int decompressedSize;
				decompressedSize = LZ4_decompress_safe((char*)compressed_data, (char*)result, (int)compressed_size, (int)decompressed_size);

				if (decompressedSize < 0 || static_cast<uint64_t>(decompressedSize) != decompressed_size)
				{
					DEBUG_LOG("LZ4 Decompression failure\n");
					delete[] result;
					return nullptr;
				}

				return result;
			}
		};
	};
}
