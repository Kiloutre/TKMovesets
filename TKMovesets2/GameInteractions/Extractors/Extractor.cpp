#include <format>
#include <fstream>
#include <cstdlib>
#include <windows.h>
//#include "lzma.h"

#include "Helpers.hpp"
#include "Extractor.hpp"

#include "GameAddresses.h"

// -- Helpers -- //

namespace ExtractorUtils
{
	uint64_t getC8AnimSize(GameProcess* process, gameAddr anim)
	{
		uint8_t bone_count = process->readInt8(anim + 2);
		uint32_t header_size = bone_count * 0x4 + 0x8;
		uint32_t frame_size = bone_count * 0x4 * 3;
		uint32_t length = process->readInt32(anim + 4);
		return (int64_t)header_size + (int64_t)frame_size * (int64_t)length;
	}

	uint32_t CalculateCrc32(std::vector<std::pair<byte*, uint64_t>>& blocks)
	{
		// Skip the first item which is always the MovesetHeader
		uint32_t crc32 = 0;
		uint32_t table[256];

		Helpers::crc32_generate_table(table);
		for (size_t i = 1; i < blocks.size(); ++i) {
			char* blockData = (char*)blocks[i].first;
			uint64_t blockSize = blocks[i].second;

			printf("block %d crc: %X\n", i,  Helpers::crc32_update(table, 0, blockData, blockSize));

			crc32 = Helpers::crc32_update(table, crc32, blockData, blockSize);
		}
		return crc32;
	}

	void WriteFileData(std::ofstream &file, std::vector<std::pair<byte*, uint64_t>>& blocks, uint8_t&progress)
	{
		uint8_t remainingProgress = 100 - progress;
		uint8_t step = remainingProgress / blocks.size();

		for (std::pair<byte*, uint64_t> block : blocks) {
			char* blockData = (char*)block.first;
			uint64_t blockSize = block.second;

			file.write(blockData, blockSize);
			Helpers::align8Bytes(file);

			progress += step;
		}
	}

	void CompressFile(std::string dest_filename, std::string src_filename)
	{
		std::rename(src_filename.c_str(), dest_filename.c_str());
		// Todo: find a fast compression algorithm, or do it on the background while leaving the uncompressed file available until the compression is done
		// Decompression speed is also really important.

		/*
		lzma_stream strm = LZMA_STREAM_INIT;
		uint32_t preset = 1;
		if (lzma_easy_encoder(&strm, preset, LZMA_CHECK_CRC64) != LZMA_OK) {
			// todo: justcopy file if compression failure
			printf("cant init lzma\n");
			return;
		}

		const unsigned int bufSize = pow(2, 20);

		printf("read size %d\n", bufSize);
		std::ifstream in(src_filename, std::ios::binary);
		std::ofstream out(dest_filename, std::ios::binary | std::ios::out);
		uint8_t* inbuf = new uint8_t[bufSize];
		uint8_t* outbuf = new uint8_t[bufSize];

		strm.next_out = outbuf;
		strm.avail_out = bufSize;
		while (true)
		{
			lzma_action action = LZMA_RUN;

			if (strm.avail_in == 0 && !in.eof()) {
				// Tell lzma where to read from (this apparently resets after every read)
				in.read((char*)inbuf, bufSize);
				strm.next_in = inbuf;
				strm.avail_in = in.gcount();
				// Still more data to read from the file, and lzma buffer is empty so nothing to write until we feed him more file data
				if (in.eof()) {
					// Input file has been read fully
					action = LZMA_FINISH;
				}
			}

			// Compress data
			lzma_ret ret = lzma_code(&strm, action);

			// Output buffer full, write it to the output file
			if (strm.avail_out == 0 || ret == LZMA_STREAM_END) {
				size_t write_size = bufSize - strm.avail_out;
				out.write((char*)outbuf, write_size);

				strm.next_out = outbuf;
				strm.avail_out = bufSize;
			}

			if (ret == LZMA_OK || ret == LZMA_STREAM_END) {
				if (ret == LZMA_STREAM_END) {
					break;
				}
			}
			else {
				printf("Error");
				break;
			}
		}
		printf("gucci\n");

		delete inbuf;
		delete outbuf;

		lzma_end(&strm);
		*/
	}
};

// Private methods //

byte* Extractor::allocateAndReadBlock(gameAddr blockStart, gameAddr blockEnd, uint64_t& size_out)
{
	size_t blockSize = blockEnd - blockStart;

	byte* block = (byte*)malloc(blockSize);
	if (block == nullptr) {
		size_out = 0;
		return nullptr;
	}

	m_process->readBytes(blockStart, block, blockSize);
	size_out = blockSize;

	return block;
}

std::string Extractor::GenerateFilename(const char* characterName, const char* gameIdentifierString, const char* extension, unsigned int suffixId)
{
	if (suffixId <= 1) {
		return std::format("{}/{}{}{}", cm_extractionDir, gameIdentifierString, characterName, extension);
	}
	else {
		return std::format("{}/{}{} ({}){}", cm_extractionDir, gameIdentifierString, characterName, suffixId, extension);
	}
}

void Extractor::GetFilepath(const char* characterName, std::string& out, std::string& out_tmp, bool overwriteSameFilename)
{
	CreateDirectory(cm_extractionDir, NULL);
	// Make sure the directory for extraction exists 

	const char* gameIdentifierstring = GetGameIdentifierString();

	std::string filePath = GenerateFilename(characterName, gameIdentifierstring, MOVESET_FILENAME_EXTENSION, 0);

	if (!overwriteSameFilename && Helpers::fileExists(filePath.c_str())) {
		// File exists: try to add numbered suffix, loop until we find a free one
		unsigned int counter = 2;
		filePath = GenerateFilename(characterName, gameIdentifierstring, MOVESET_FILENAME_EXTENSION, counter);

		while (Helpers::fileExists(filePath.c_str())) {
			filePath = GenerateFilename(characterName, gameIdentifierstring, MOVESET_FILENAME_EXTENSION, counter);
			counter++;
		}

		out = filePath;
		out_tmp = GenerateFilename(characterName, gameIdentifierstring, MOVESET_TMPFILENAME_EXTENSION, counter);
	}
	else {
		// File doesnt exist or we are allowed to overwrite, no need to suffix
		out = filePath;
		out_tmp = GenerateFilename(characterName, gameIdentifierstring, MOVESET_TMPFILENAME_EXTENSION);
	}
}
