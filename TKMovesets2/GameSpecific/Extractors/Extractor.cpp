#include <stdexcept>
#include <format>
#include <fstream>
#include <cstdlib>
#include <windows.h>
#include <filesystem>

#include "Helpers.hpp"
#include "Extractor.hpp"
#include "MovesetFolders.hpp"
#include "Animations.hpp"

#include "MovesetStructs.h"
#include "GameTypes.h"

using namespace ByteswapHelpers;

// -- Helpers -- //

namespace ExtractorUtils
{

	void WriteFileData(std::ofstream &file, const std::vector<std::pair<Byte*, uint64_t>>& blocks, uint8_t&progress, uint8_t progress_max)
	{
		uint8_t remainingProgress = progress_max - progress;
		uint8_t step = (uint8_t)((size_t)remainingProgress / blocks.size());

		for (const std::pair<Byte*, uint64_t>& block : blocks) {
			char* blockData = (char*)block.first;
			uint64_t blockSize = block.second;

			file.write(blockData, blockSize);
			Helpers::align8Bytes(file);

			progress += step;
		}
	}

	TKMovesetCompressionType_ GetCompressionAlgorithm(ExtractSettings settings)
	{
		if (settings & ExtractSettings_CompressCompactZstd) return TKMovesetCompressionType_CompactZstd;
		if (settings & ExtractSettings_CompressLZMA) return TKMovesetCompressionType_LZMA;
		if (settings & ExtractSettings_CompressLZ4) return TKMovesetCompressionType_LZ4;
		return TKMovesetCompressionType_None;
	}
};

// Private methods //

Byte* Extractor::allocateAndReadBlock(gameAddr blockStart, gameAddr blockEnd, uint64_t& size_out)
{
	size_out = 0;
	if (blockEnd < blockStart) {
		DEBUG_LOG("Extraction error: block end (%llx) smaller than block start (%llx)\n", blockEnd, blockStart);
		return nullptr;
	}

	size_t blockSize = blockEnd - blockStart;

	if (blockSize >= (MOVESET_MAX_BLOCK_SIZE_MB * 1000000)) { // 50 MB
		DEBUG_LOG("Block size too big to be valid (%llu / 0x%llx), returning nullptr\n", blockSize, blockSize);
		
#ifdef BUILD_TYPE_DEBUG
		DebugBreak();
#endif
		// Arbitrary max block size in case something bad happened and we get a very invalid start & end
		return nullptr;
	}

	Byte* block = (Byte*)malloc(blockSize);
	if (block == nullptr) {
		size_out = 0;
		return nullptr;
	}

	if (!m_game.ReadBytes(blockStart, block, blockSize)) {
		free(block);
		return nullptr;
	}
	size_out = blockSize;

	return block;
}

std::wstring Extractor::GenerateFilename(const char* characterName, const char* gameIdentifierString, const char* extension, const std::filesystem::path& directory, unsigned int suffixId)
{
	if (suffixId <= 1) {
		std::wstring outputFilename = directory.wstring() + L"/" +
			Helpers::string_to_wstring(gameIdentifierString) + Helpers::string_to_wstring(characterName)
			+ Helpers::string_to_wstring(extension);
		return outputFilename;
	}
	else {
		std::wstring outputFilename = directory.wstring() + L"/" +
			Helpers::string_to_wstring(gameIdentifierString) + Helpers::string_to_wstring(characterName)
			+ L" (" + std::to_wstring(suffixId) + L")" + Helpers::string_to_wstring(extension);
		return outputFilename;
	}
}

void Extractor::GetFilepath(const char* characterName, std::wstring& out, std::wstring& out_tmp, bool overwriteSameFilename)
{
    const auto directory=std::string_view(cm_extractionDir)==MOVESET_DIRECTORY ? MovesetFolders::ExtractionDirectory() : std::filesystem::u8path(cm_extractionDir);
    std::filesystem::create_directories(directory);
	// Make sure the directory for extraction exists 

	const char* gameIdentifierstring = m_gameInfo.movesetNamePrefix;

	std::wstring filePath = GenerateFilename(characterName, gameIdentifierstring, MOVESET_FILENAME_EXTENSION, directory, 0);

	if (!overwriteSameFilename && Helpers::fileExists(filePath.c_str())) {
		// File exists: try to add numbered suffix, loop until we find a free one
		unsigned int counter = 2;
		filePath = GenerateFilename(characterName, gameIdentifierstring, MOVESET_FILENAME_EXTENSION, directory, counter);

		while (Helpers::fileExists(filePath.c_str())) {
			filePath = GenerateFilename(characterName, gameIdentifierstring, MOVESET_FILENAME_EXTENSION, directory, counter);
			counter++;
		}

		out = filePath;
		out_tmp = GenerateFilename(characterName, gameIdentifierstring, MOVESET_TMPFILENAME_EXTENSION, directory, counter);
	}
	else {
		// File doesnt exist or we are allowed to overwrite, no need to suffix
		out = filePath;
		out_tmp = GenerateFilename(characterName, gameIdentifierstring, MOVESET_TMPFILENAME_EXTENSION, directory);
	}
}

uint64_t Extractor::GetAnimationSize(gameAddr anim)
{
    uint16_t tag = 0;
    if (!m_process.readBytes(anim, &tag, sizeof(tag)))
        throw std::runtime_error("Cannot read animation header");
    uint64_t size = 0;
    if (tag == 0xc8 || tag == 0xc800) size = TAnimUtils::FromProcess::getC8AnimSize(m_process, anim);
    if (tag == 0x64 || tag == 0x6400) size = TAnimUtils::FromProcess::get64AnimSize(m_process, anim);
    if (!size) throw std::runtime_error("Invalid or unreadable animation");
    return size;
}
