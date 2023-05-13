#include <lz4.h>
#include <filesystem>
#include <fstream>
#include <lzma.h>

#include "Compression.hpp"

#include "MovesetStructs.h"

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
	}
};
const size_t g_compressionTypes_len = _countof(g_compressionTypes);

// -- Lzma utils -- //

static bool lzma_init_encoder(lzma_stream* strm, uint32_t preset)
{
	// Initialize the encoder using a preset. Set the integrity to check
	// to CRC64, which is the default in the xz command line tool. If
	// the .xz file needs to be decompressed with XZ Embedded, use
	// LZMA_CHECK_CRC32 instead.
	lzma_ret ret = lzma_easy_encoder(strm, preset, LZMA_CHECK_CRC64);

	// Return successfully if the initialization went fine.
	if (ret == LZMA_OK)
		return true;

	// Something went wrong. The possible errors are documented in
	// lzma/container.h (src/liblzma/api/lzma/container.h in the source
	// package or e.g. /usr/include/lzma/container.h depending on the
	// install prefix).
	const char* msg;
	switch (ret) {
	case LZMA_MEM_ERROR:
		msg = "Memory allocation failed";
		break;

	case LZMA_OPTIONS_ERROR:
		msg = "Specified preset is not supported";
		break;

	case LZMA_UNSUPPORTED_CHECK:
		msg = "Specified integrity check is not supported";
		break;

	default:
		// This is most likely LZMA_PROG_ERROR indicating a bug in
		// this program or in liblzma. It is inconvenient to have a
		// separate error message for errors that should be impossible
		// to occur, but knowing the error code is important for
		// debugging. That's why it is good to print the error code
		// at least when there is no good error message to show.
		msg = "Unknown error, possibly a bug";
		break;
	}

	fprintf(stderr, "Error initializing the encoder: %s (error code %u)\n",
		msg, ret);
	return false;
}


static bool lzma_compress(lzma_stream* strm, Byte* input_data, uint64_t input_size, Byte* output_data, int32_t& size_out)
{
	// This will be LZMA_RUN until the end of the input file is reached.
	// This tells lzma_code() when there will be no more input.
	lzma_action action = LZMA_RUN;

	// Initialize the input and output pointers. Initializing next_in
	// and avail_in isn't really necessary when we are going to encode
	// just one file since LZMA_STREAM_INIT takes care of initializing
	// those already. But it doesn't hurt much and it will be needed
	// if encoding more than one file like we will in 02_decompress.c.
	//
	// While we don't care about strm->total_in or strm->total_out in this
	// example, it is worth noting that initializing the encoder will
	// always reset total_in and total_out to zero. But the encoder
	// initialization doesn't touch next_in, avail_in, next_out, or
	// avail_out.

	uint64_t output_buffer_size = input_size;

	strm->next_in = NULL;
	strm->avail_in = 0;
	strm->next_out = output_data;
	strm->avail_out = output_buffer_size;

	// Loop until the file has been successfully compressed or until
	// an error occurs.

	size_out = 0;

	uint64_t remainingBytes = input_size;

	while (true) {
		// Fill the input buffer if it is empty.
		if (strm->avail_in == 0 && remainingBytes > 0) {
			strm->next_in = input_data;
			strm->avail_in = BUFSIZ < remainingBytes ? BUFSIZ : remainingBytes;

			remainingBytes -= strm->avail_in;
			input_data += strm->avail_in;

			// Once the end of the input file has been reached,
			// we need to tell lzma_code() that no more input
			// will be coming and that it should finish the
			// encoding.
			if (remainingBytes == 0)
				action = LZMA_FINISH;
		}

		// Tell liblzma do the actual encoding.
		//
		// This reads up to strm->avail_in bytes of input starting
		// from strm->next_in. avail_in will be decremented and
		// next_in incremented by an equal amount to match the
		// number of input bytes consumed.
		//
		// Up to strm->avail_out bytes of compressed output will be
		// written starting from strm->next_out. avail_out and next_out
		// will be incremented by an equal amount to match the number
		// of output bytes written.
		//
		// The encoder has to do internal buffering, which means that
		// it may take quite a bit of input before the same data is
		// available in compressed form in the output buffer.
		lzma_ret ret = lzma_code(strm, action);

		// If the output buffer is full or if the compression finished
		// successfully, write the data from the output bufffer to
		// the output file.
		if (strm->avail_out == 0 || ret == LZMA_STREAM_END) {
			// When lzma_code() has returned LZMA_STREAM_END,
			// the output buffer is likely to be only partially
			// full. Calculate how much new data there is to
			// be written to the output file.
			size_t write_size = output_buffer_size - strm->avail_out;

			size_out += (int)write_size;
			output_data += write_size;

			// Reset next_out and avail_out.
			strm->next_out = output_data;
			strm->avail_out = output_buffer_size;
		}

		// Normally the return value of lzma_code() will be LZMA_OK
		// until everything has been encoded.
		if (ret != LZMA_OK) {
			// Once everything has been encoded successfully, the
			// return value of lzma_code() will be LZMA_STREAM_END.
			//
			// It is important to check for LZMA_STREAM_END. Do not
			// assume that getting ret != LZMA_OK would mean that
			// everything has gone well.
			if (ret == LZMA_STREAM_END)
				return true;

			// It's not LZMA_OK nor LZMA_STREAM_END,
			// so it must be an error code. See lzma/base.h
			// (src/liblzma/api/lzma/base.h in the source package
			// or e.g. /usr/include/lzma/base.h depending on the
			// install prefix) for the list and documentation of
			// possible values. Most values listen in lzma_ret
			// enumeration aren't possible in this example.
			const char* msg;
			switch (ret) {
			case LZMA_MEM_ERROR:
				msg = "Memory allocation failed";
				break;

			case LZMA_DATA_ERROR:
				// This error is returned if the compressed
				// or uncompressed size get near 8 EiB
				// (2^63 bytes) because that's where the .xz
				// file format size limits currently are.
				// That is, the possibility of this error
				// is mostly theoretical unless you are doing
				// something very unusual.
				//
				// Note that strm->total_in and strm->total_out
				// have nothing to do with this error. Changing
				// those variables won't increase or decrease
				// the chance of getting this error.
				msg = "File size limits exceeded";
				break;

			default:
				// This is most likely LZMA_PROG_ERROR, but
				// if this program is buggy (or liblzma has
				// a bug), it may be e.g. LZMA_BUF_ERROR or
				// LZMA_OPTIONS_ERROR too.
				//
				// It is inconvenient to have a separate
				// error message for errors that should be
				// impossible to occur, but knowing the error
				// code is important for debugging. That's why
				// it is good to print the error code at least
				// when there is no good error message to show.
				msg = "Unknown error, possibly a bug";
				break;
			}

			DEBUG_LOG("Encoder error: %s (error code %u)\n", msg, ret);
			return false;
		}
	}
}
static bool lzma_init_decoder(lzma_stream* strm)
{
	// Initialize a .xz decoder. The decoder supports a memory usage limit
	// and a set of flags.
	//
	// The memory usage of the decompressor depends on the settings used
	// to compress a .xz file. It can vary from less than a megabyte to
	// a few gigabytes, but in practice (at least for now) it rarely
	// exceeds 65 MiB because that's how much memory is required to
	// decompress files created with "xz -9". Settings requiring more
	// memory take extra effort to use and don't (at least for now)
	// provide significantly better compression in most cases.
	//
	// Memory usage limit is useful if it is important that the
	// decompressor won't consume gigabytes of memory. The need
	// for limiting depends on the application. In this example,
	// no memory usage limiting is used. This is done by setting
	// the limit to UINT64_MAX.
	//
	// The .xz format allows concatenating compressed files as is:
	//
	//     echo foo | xz > foobar.xz
	//     echo bar | xz >> foobar.xz
	//
	// When decompressing normal standalone .xz files, LZMA_CONCATENATED
	// should always be used to support decompression of concatenated
	// .xz files. If LZMA_CONCATENATED isn't used, the decoder will stop
	// after the first .xz stream. This can be useful when .xz data has
	// been embedded inside another file format.
	//
	// Flags other than LZMA_CONCATENATED are supported too, and can
	// be combined with bitwise-or. See lzma/container.h
	// (src/liblzma/api/lzma/container.h in the source package or e.g.
	// /usr/include/lzma/container.h depending on the install prefix)
	// for details.
	lzma_ret ret = lzma_stream_decoder(
		strm, UINT64_MAX, LZMA_CONCATENATED);

	// Return successfully if the initialization went fine.
	if (ret == LZMA_OK)
		return true;

	// Something went wrong. The possible errors are documented in
	// lzma/container.h (src/liblzma/api/lzma/container.h in the source
	// package or e.g. /usr/include/lzma/container.h depending on the
	// install prefix).
	//
	// Note that LZMA_MEMLIMIT_ERROR is never possible here. If you
	// specify a very tiny limit, the error will be delayed until
	// the first headers have been parsed by a call to lzma_code().
	const char* msg;
	switch (ret) {
	case LZMA_MEM_ERROR:
		msg = "Memory allocation failed";
		break;

	case LZMA_OPTIONS_ERROR:
		msg = "Unsupported decompressor flags";
		break;

	default:
		// This is most likely LZMA_PROG_ERROR indicating a bug in
		// this program or in liblzma. It is inconvenient to have a
		// separate error message for errors that should be impossible
		// to occur, but knowing the error code is important for
		// debugging. That's why it is good to print the error code
		// at least when there is no good error message to show.
		msg = "Unknown error, possibly a bug";
		break;
	}

	DEBUG_LOG("Error initializing the decoder: %s (error code %u)\n", msg, ret);
	return false;
}


static bool lzma_decompress(lzma_stream* strm, Byte* compressed_data, int32_t compressed_size, Byte* output, uint64_t decompressed_size)
{
	// When LZMA_CONCATENATED flag was used when initializing the decoder,
	// we need to tell lzma_code() when there will be no more input.
	// This is done by setting action to LZMA_FINISH instead of LZMA_RUN
	// in the same way as it is done when encoding.
	//
	// When LZMA_CONCATENATED isn't used, there is no need to use
	// LZMA_FINISH to tell when all the input has been read, but it
	// is still OK to use it if you want. When LZMA_CONCATENATED isn't
	// used, the decoder will stop after the first .xz stream. In that
	// case some unused data may be left in strm->next_in.
	lzma_action action = LZMA_RUN;

	strm->next_in = NULL;
	strm->avail_in = 0;
	strm->next_out = output;
	strm->avail_out = decompressed_size;

	uint64_t remainingBytes = compressed_size;

	while (true) {
		if (strm->avail_in == 0 && remainingBytes > 0) {
			strm->next_in = compressed_data;
			strm->avail_in = remainingBytes;// BUFSIZ < remainingBytes ? BUFSIZ : remainingBytes;

			compressed_data += strm->avail_in;
			remainingBytes -= strm->avail_in;

			// Once the end of the input file has been reached,
			// we need to tell lzma_code() that no more input
			// will be coming. As said before, this isn't required
			// if the LZMA_CONATENATED flag isn't used when
			// initializing the decoder.
			if (remainingBytes == 0)
				action = LZMA_FINISH;
		}

		lzma_ret ret = lzma_code(strm, action);

		if (strm->avail_out == 0 || ret == LZMA_STREAM_END) {
			strm->next_out = output;
			strm->avail_out = 0;
		}

		if (ret != LZMA_OK) {
			// Once everything has been decoded successfully, the
			// return value of lzma_code() will be LZMA_STREAM_END.
			//
			// It is important to check for LZMA_STREAM_END. Do not
			// assume that getting ret != LZMA_OK would mean that
			// everything has gone well or that when you aren't
			// getting more output it must have successfully
			// decoded everything.
			if (ret == LZMA_STREAM_END)
				return true;

			// It's not LZMA_OK nor LZMA_STREAM_END,
			// so it must be an error code. See lzma/base.h
			// (src/liblzma/api/lzma/base.h in the source package
			// or e.g. /usr/include/lzma/base.h depending on the
			// install prefix) for the list and documentation of
			// possible values. Many values listen in lzma_ret
			// enumeration aren't possible in this example, but
			// can be made possible by enabling memory usage limit
			// or adding flags to the decoder initialization.
			const char* msg;
			switch (ret) {
			case LZMA_MEM_ERROR:
				msg = "Memory allocation failed";
				break;

			case LZMA_FORMAT_ERROR:
				// .xz magic bytes weren't found.
				msg = "The input is not in the .xz format";
				break;

			case LZMA_OPTIONS_ERROR:
				// For example, the headers specify a filter
				// that isn't supported by this liblzma
				// version (or it hasn't been enabled when
				// building liblzma, but no-one sane does
				// that unless building liblzma for an
				// embedded system). Upgrading to a newer
				// liblzma might help.
				//
				// Note that it is unlikely that the file has
				// accidentally became corrupt if you get this
				// error. The integrity of the .xz headers is
				// always verified with a CRC32, so
				// unintentionally corrupt files can be
				// distinguished from unsupported files.
				msg = "Unsupported compression options";
				break;

			case LZMA_DATA_ERROR:
				msg = "Compressed file is corrupt";
				break;

			case LZMA_BUF_ERROR:
				// Typically this error means that a valid
				// file has got truncated, but it might also
				// be a damaged part in the file that makes
				// the decoder think the file is truncated.
				// If you prefer, you can use the same error
				// message for this as for LZMA_DATA_ERROR.
				msg = "Compressed file is truncated or "
					"otherwise corrupt";
				break;

			default:
				// This is most likely LZMA_PROG_ERROR.
				msg = "Unknown error, possibly a bug";
				break;
			}

			DEBUG_LOG("Decoder error: %s (error code %u)\n", msg, ret);
			return false;
		}
	}
}

// -- -- //

namespace CompressionUtils
{
	const s_compressionTypes& GetCompressionSetting(uint8_t idx)
	{
		if (idx >= g_compressionTypes_len) {
			DEBUG_LOG("Bad compression setting: %u\n", idx);
			throw;
		}

		return g_compressionTypes[idx];
	}

	size_t GetCompressionSettingCount()
	{
		return g_compressionTypes_len;
	}

	unsigned int GetDefaultCompressionSetting()
	{
		for (unsigned int i = 0; i < g_compressionTypes_len; ++i) {
			if (g_compressionTypes[i].compressionSetting == TKMovesetCompressionType_LZMA) {
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
			bool Compress(const std::wstring& dest_filename, const std::wstring& src_filename, TKMovesetCompressionType_ compressionType)
			{
				std::ifstream orig_file(src_filename, std::ios::binary);
				std::ofstream new_file(dest_filename, std::ios::binary);

				// Copy up to moveset_data_start, get size of moveset data
				int32_t moveset_data_size;
				{
					// Read header data
					TKMovesetHeader header;
					orig_file.read((char*)&header, sizeof(header));

					uint32_t moveset_data_start = header.moveset_data_start;

					// Read up to moveset data start (includes header and more)
					char* buf = new char[moveset_data_start];
					orig_file.seekg(0, std::ios::beg);
					orig_file.read(buf, moveset_data_start);

					// Calculate moveset data size
					orig_file.seekg(0, std::ios::end);
					moveset_data_size = (uint32_t)orig_file.tellg() - moveset_data_start;

					// Mark moveset as compressed
					((TKMovesetHeader*)buf)->compressionType = compressionType;
					((TKMovesetHeader*)buf)->moveset_data_size = moveset_data_size;

					// Write header
					new_file.write(buf, moveset_data_start);
					delete[] buf;

					// Move cursor back to the start of the moveset data
					orig_file.seekg(moveset_data_start, std::ios::beg);
				}

				// Compress moveset data
				Byte* inbuf = new Byte[moveset_data_size];
				Byte* outbuf = nullptr;
				orig_file.read((char*)inbuf, moveset_data_size);
				orig_file.close();

				std::filesystem::remove(src_filename);
				int32_t compressed_size = 0;

				switch (compressionType)
				{
				case TKMovesetCompressionType_LZ4:
					DEBUG_LOG("Using LZ4 compression type...\n");
					outbuf = CompressionUtils::RAW::LZ4::Compress(inbuf, moveset_data_size, compressed_size);
					break;
				case TKMovesetCompressionType_LZMA:
					DEBUG_LOG("Using LZMA compression type...\n");
					outbuf = CompressionUtils::RAW::LZMA::Compress(inbuf, moveset_data_size, compressed_size, 0);
					break;

				default:
					DEBUG_LOG("Unhandled compression type '%u'\n", compressionType);
					break;
				}

				DEBUG_LOG("Compression: Old size was %d, compressed size is %d\n", moveset_data_size, compressed_size);

				if (outbuf == nullptr)
				{
					new_file.close();
					std::filesystem::remove(dest_filename);
					DEBUG_LOG("Compression failure\n");
				}
				else {
					new_file.write((char*)outbuf, compressed_size);
				}

				delete[] inbuf;
				delete[] outbuf;
				return compressed_size >= 0;
			}
		};
	};

	namespace RAW
	{
		namespace Moveset
		{
			Byte* Decompress(Byte* moveset, int32_t compressed_size, uint64_t& size_out)
			{
				TKMovesetHeader* header = (TKMovesetHeader*)moveset;

				if (header->compressionType == TKMovesetCompressionType_None || header->moveset_data_size <= 0) {
					return nullptr;
				}

				Byte* moveset_data_start = moveset + header->moveset_data_start;
				Byte* result = nullptr;

				switch (header->compressionType)
				{
				case TKMovesetCompressionType_LZ4:
					result = CompressionUtils::RAW::LZ4::Decompress(moveset_data_start, compressed_size, header->moveset_data_size);
					break;
				case TKMovesetCompressionType_LZMA:
					result = CompressionUtils::RAW::LZMA::Decompress(moveset_data_start, compressed_size, header->moveset_data_size);
					break;
				default:
					DEBUG_LOG("!! Moveset decompression: unhandled compression type '%u' !!\n", header->compressionType);
					return nullptr;
					break;
				}

				if (result == nullptr) {
					DEBUG_LOG("!! Error during decompression !!\n");
				}

				return result;
			}
		};

		namespace LZMA
		{
			Byte* Compress(Byte* input_data, int32_t input_size, int32_t& size_out, uint8_t preset)
			{
				Byte* result;
				size_out = 0;

				if (preset > 9) {
					DEBUG_LOG("CompressDataLzma: Preset %u is above the maximum preset (9)\n", preset);
					preset = 9;
				}

				lzma_stream strm = LZMA_STREAM_INIT;
				if (!lzma_init_encoder(&strm, preset)) {
					DEBUG_LOG("LZMA encoder init failure\n");
					return nullptr;
				}

				result = new Byte[input_size];
				if (!lzma_compress(&strm, input_data, input_size, result, size_out))
				{
					DEBUG_LOG("LZMA Compression failure\n");
					delete[] result;
					result = nullptr;
					size_out = 0;
				}

				lzma_end(&strm);

				return result;
			}

			Byte* Decompress(Byte* compressed_data, int32_t compressed_size, int32_t decompressed_size)
			{
				Byte* result = nullptr;

				lzma_stream strm = LZMA_STREAM_INIT;

				if (!lzma_init_decoder(&strm)) {
					DEBUG_LOG("LZMA decoder init failure\n");
					return nullptr;
				}

				result = new Byte[decompressed_size];
				if (!lzma_decompress(&strm, compressed_data, compressed_size, result, decompressed_size))
				{
					DEBUG_LOG("LZMA Decompression failure\n");
					delete[] result;
					result = nullptr;
				}

				lzma_end(&strm);

				return result;
			}
		}

		namespace LZ4
		{
			Byte* Compress(Byte* decompressed_data, int32_t decompressed_size, int32_t& size_out)
			{
				size_out = 0;
				Byte* result = new Byte[decompressed_size];

				int compressed_size = LZ4_compress_default((char*)decompressed_data, (char*)result, (int)decompressed_size, (int)decompressed_size);

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

			Byte* Decompress(Byte* compressed_data, int32_t compressed_size, int32_t decompressed_size)
			{
				Byte* result = new Byte[decompressed_size];

				int decompressedSize;
				decompressedSize = LZ4_decompress_safe((char*)compressed_data, (char*)result, compressed_size, decompressed_size);

				if (decompressedSize <= 0)
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