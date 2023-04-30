#include <format>
#include <fstream>
#include <cstdlib>
#include <windows.h>
#include <filesystem>
#include <lz4.h>

#include "Helpers.hpp"
#include "Extractor.hpp"

#include "MovesetStructs.h"
#include "GameTypes.h"

using namespace ByteswapHelpers;

// -- Helpers -- //

namespace ExtractorUtils
{
	uint64_t getC8AnimSize(GameProcess* process, gameAddr anim)
	{
		bool isSwapped = process->readInt8(anim + 0) == 0;

		uint8_t bone_count = process->readInt8(anim + (isSwapped ? 3 : 2));
		uint32_t header_size = bone_count * 0x4 + 0x8;
		uint32_t frame_size = bone_count * 0x4 * 3;

		uint32_t length = process->readInt32(anim + 4);
		if (isSwapped) {
			length = BYTESWAP_INT32(length);
		}

		return (int64_t)header_size + (int64_t)frame_size * (int64_t)length;
	}

	uint64_t get64AnimSize(GameProcess* process, gameAddr anim)
	{
		bool isSwapped = process->readInt8(anim + 0) == 0;
		return isSwapped ? get64AnimSize_BigEndian(process,anim) : get64AnimSize_LittleEndian(process, anim);
	}

	uint64_t get64AnimSize_BigEndian(GameProcess* process, gameAddr anim)
	{
		// Do all calculations in uint64_t that way i don't have to pay attention to possible overflows

		uint64_t boneCount = process->readInt16(anim + 2);
		boneCount = BYTESWAP_INT16(boneCount);

		uint64_t postBoneDescriptor_offset = (4 + boneCount * sizeof(uint16_t));
		gameAddr anim_postBoneDescriptorAddr = (gameAddr)(anim + postBoneDescriptor_offset);

		uint64_t animLength = (uint16_t)process->readInt16(anim_postBoneDescriptorAddr);
		uint64_t __unknown__ = (uint16_t)process->readInt16(anim_postBoneDescriptorAddr + 4);
		animLength = BYTESWAP_INT16(animLength);
		__unknown__ = BYTESWAP_INT16(__unknown__);

		uint64_t vv73 = 2 * ((4 * __unknown__ + 6) / 2);
		uint64_t aa4 = 6 * (__unknown__ + boneCount);

		gameAddr animPtr = anim_postBoneDescriptorAddr + vv73 + aa4;

		unsigned int baseFrame = (unsigned int)animLength - (animLength >= 2 ? 2 : 1);
		unsigned int keyframe = baseFrame / 16;
		unsigned int _v56_intPtr = (unsigned int)process->readInt32(animPtr + 4 * (uint64_t)keyframe);
		_v56_intPtr = BYTESWAP_INT32(_v56_intPtr);

		gameAddr animPtr_2 = animPtr + _v56_intPtr;
		int lastArg_copy = (int)boneCount;

		do
		{
			for (int i = 0; i < 3; ++i)
			{
				Byte v58 = process->readInt8(animPtr_2);
				int offsetStep = v58 / 4;
				animPtr_2 += offsetStep;
			}
		} while (--lastArg_copy != 0);

		return (uint64_t)animPtr_2 - (uint64_t)anim;
	}

	uint64_t get64AnimSize_LittleEndian(GameProcess* process, gameAddr anim)
	{
		// Do all calculations in uint64_t that way i don't have to pay attention to possible overflows

		uint64_t boneCount = process->readInt16(anim + 2);

		uint64_t postBoneDescriptor_offset = (4 + boneCount * sizeof(uint16_t));
		gameAddr anim_postBoneDescriptorAddr = (gameAddr)(anim + postBoneDescriptor_offset);

		uint64_t animLength = (uint16_t)process->readInt16(anim_postBoneDescriptorAddr);
		uint64_t __unknown__ = (uint16_t)process->readInt16(anim_postBoneDescriptorAddr + 4);

		uint64_t vv73 = 2 * ((4 * __unknown__ + 6) / 2);
		uint64_t aa4 = 6 * (__unknown__ + boneCount);

		gameAddr animPtr = anim_postBoneDescriptorAddr + vv73 + aa4;

		unsigned int baseFrame = (unsigned int)animLength - (animLength >= 2 ? 2 : 1);
		unsigned int keyframe = baseFrame / 16;
		unsigned int _v56_intPtr = (unsigned int)process->readInt32(animPtr + 4 * (uint64_t)keyframe);

		gameAddr animPtr_2 = animPtr + _v56_intPtr;
		int lastArg_copy = (int)boneCount;

		do
		{
			for (int i = 0; i < 3; ++i)
			{
				Byte v58 = process->readInt8(animPtr_2);
				int offsetStep = v58 / 4;
				animPtr_2 += offsetStep;
			}
		} while (--lastArg_copy != 0);

		return (uint64_t)animPtr_2 - (uint64_t)anim;
	}

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

	void ByteswapMota(Byte* motaAddr)
	{
		bool sourceIsBigEndian = motaAddr[5] == 1;
		bool targetIsBigEndian = !sourceIsBigEndian;

		uint32_t animCount = DEREF_UINT32(motaAddr + 0xC);
		if (sourceIsBigEndian) {
			animCount = BYTESWAP_INT32(animCount);
		}

		SWAP_SHORT(motaAddr + 4); // Swap is_little_endian & is_big_endian values
		SWAP_SHORT(motaAddr + 6);

		SWAP_INT(motaAddr + 8);
		SWAP_INT(motaAddr + 0xC); // Swap anim count

		for (unsigned int i = 0; i < animCount; ++i)
		{
			int listOffset = 0x14 + (i * sizeof(int));

			uint32_t animOffset = DEREF_UINT32(motaAddr + listOffset);
			int realAnimOffset = sourceIsBigEndian ? BYTESWAP_INT32(animOffset) : animOffset;

			SWAP_INT(motaAddr + listOffset);
			ByteswapAnimation(motaAddr + realAnimOffset);
		}
	}

	void Byteswap64Animation(Byte* animAddr)
	{
		bool isBoneInt16[256]; // Stack indicating whether a specific bone is 3x short or 3x floats

		bool sourceIsBigEndian = animAddr[0] == 0;
		bool targetIsBigEndian = !sourceIsBigEndian;

		// Get important informations
		uint16_t boneCount = DEREF_UINT16(animAddr + 2);
		if (sourceIsBigEndian) {
			boneCount = BYTESWAP_INT16(boneCount);
		}
		uint16_t animLength = DEREF_UINT16(animAddr + 4 + boneCount * 2);
		if (sourceIsBigEndian) {
			animLength = BYTESWAP_INT16(animLength);
		}

		SWAP_SHORT(animAddr); // Byteswap anim type
		SWAP_SHORT(animAddr + 2); // Byteswap bone count

		// Swap bone descriptor list and fill isBoneInt16;
		for (unsigned int i = 0; i < boneCount; ++i)
		{
			int descriptorOffset = 4 + (i * sizeof(uint16_t));
			uint16_t descriptor = DEREF_UINT16(animAddr + descriptorOffset);
			uint16_t realDescriptor = sourceIsBigEndian ? BYTESWAP_INT16(descriptor) : descriptor;

			isBoneInt16[i] = realDescriptor - 4 < 4;

			SWAP_SHORT(animAddr + descriptorOffset);
		}


		uint16_t offset = 4 + boneCount * sizeof(uint16_t);

		uint16_t iterCount = DEREF_UINT16(animAddr + offset + 4);
		if (sourceIsBigEndian) {
			iterCount = BYTESWAP_INT16(iterCount);
		}

		SWAP_SHORT(animAddr + offset); // Swap anim length
		//SWAP_SHORT(animAddr + boneCount + 2); // This one isn't actually done, not in my old working byteswap scripts and not in T7's code either.
		SWAP_SHORT(animAddr + offset + 4); // Swap iter count

		offset += 6;
		for (int i = 0; i < iterCount; ++i) {
			SWAP_INT(animAddr + offset);
			offset += sizeof(int);
		}

		// Swap base position
		for (unsigned int i = 0; i < boneCount; ++i)
		{
			if (isBoneInt16[i])
			{
				SWAP_SHORT(animAddr + offset);
				SWAP_SHORT(animAddr + offset + 2);
				SWAP_SHORT(animAddr + offset + 4);
				offset += sizeof(int16_t) * 3;
			}
			else
			{
				SWAP_INT(animAddr + offset);
				SWAP_INT(animAddr + offset + 4);
				SWAP_INT(animAddr + offset + 8);
				offset += sizeof(float) * 3;
			}
		}

		// Byteswap keyframe informations
		int keyframeCount = (animLength + 0xE) >> 4;
		while (keyframeCount-- > 0)
		{
			SWAP_INT(animAddr + offset);
			offset += 4;
		}
	}

	void ByteswapC8Animation(Byte* animAddr)
	{
		bool sourceIsBigEndian = animAddr[0] == 0;
		bool targetIsBigEndian = !sourceIsBigEndian;

		uint16_t boneCount = DEREF_UINT16(animAddr + 2);
		uint32_t animLength = DEREF_UINT32(animAddr + 4);
		if (sourceIsBigEndian) {
			boneCount = BYTESWAP_INT16(boneCount);
			animLength = BYTESWAP_INT32(animLength);
		}

		SWAP_SHORT(animAddr + 0); // Swap anim type
		SWAP_SHORT(animAddr + 2); // Swap bone count

		SWAP_INT(animAddr + 4); // Swap anim length

		// Swap bone descriptor list
		for (unsigned int i = 0; i < boneCount; ++i)
		{
			int descriptorOffset = 8 + (i * sizeof(int));
			SWAP_INT(animAddr + descriptorOffset);
		}

		// Swap every bone value (float)
		int headerSize = 8 + boneCount * 4;
		float* bonePtr = (float*)(animAddr + headerSize);

		for (unsigned int frame = 0; frame < animLength; ++frame)
		{
			for (unsigned int i = 0; i < boneCount; ++i)
			{
				SWAP_INT(bonePtr);
				++bonePtr;
			}
		}
	}

	void ByteswapAnimation(Byte* animAddr)
	{
		Byte animType = animAddr[0] == 0 ? animAddr[1] : animAddr[0];
		switch (animType)
		{
		case 0x64:
			Byteswap64Animation(animAddr);
			break;
		case 0xC8:
			ByteswapC8Animation(animAddr);
			break;
		default:
			DEBUG_LOG("!! ByteswapAnimation() ERROR: INVALID ANIMATION TYPE '%x' FOR ADDR '%p' !!\n", animType, animAddr);
			throw;
			break;
		}
	}
};

// Private methods //

Byte* Extractor::allocateAndReadBlock(gameAddr blockStart, gameAddr blockEnd, uint64_t& size_out)
{
	size_t blockSize = blockEnd - blockStart;

	if (blockSize >= (MOVESET_MAX_BLOCK_SIZE_MB * 1000000)) { // 50 MB
		DEBUG_LOG("Block size too big to be valid, returning nullptr\n");
		// Arbitrary max block size in case something bad happened and we get a very invalid start & end
		return nullptr;
	}

	Byte* block = (Byte*)malloc(blockSize);
	if (block == nullptr) {
		size_out = 0;
		return nullptr;
	}

	m_process->readBytes(blockStart, block, blockSize);
	size_out = blockSize;

	return block;
}

std::wstring Extractor::GenerateFilename(const char* characterName, const char* gameIdentifierString, const char* extension, unsigned int suffixId)
{
	if (suffixId <= 1) {
		std::wstring outputFilename = Helpers::string_to_wstring(cm_extractionDir) + L"/" +
			Helpers::string_to_wstring(gameIdentifierString) + Helpers::string_to_wstring(characterName)
			+ Helpers::string_to_wstring(extension);
		return outputFilename;
	}
	else {
		std::wstring outputFilename = Helpers::string_to_wstring(cm_extractionDir) + L"/" +
			Helpers::string_to_wstring(gameIdentifierString) + Helpers::string_to_wstring(characterName)
			+ L" (" + std::to_wstring(suffixId) + L")" + Helpers::string_to_wstring(extension);
		return outputFilename;
	}
}

void Extractor::GetFilepath(const char* characterName, std::wstring& out, std::wstring& out_tmp, bool overwriteSameFilename)
{
	CreateDirectory(cm_extractionDir, NULL);
	// Make sure the directory for extraction exists 

	const char* gameIdentifierstring = m_gameInfo.movesetNamePrefix;

	std::wstring filePath = GenerateFilename(characterName, gameIdentifierstring, MOVESET_FILENAME_EXTENSION, 0);

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

uint64_t Extractor::GetAnimationSize(gameAddr anim)
{
	uint16_t animType = m_process->readInt16(anim);
	if ((animType & 0xFF) == 0) {
		animType = BYTESWAP_INT16(animType);
	}

	if (animType == 0xC8) {
		return ExtractorUtils::getC8AnimSize(m_process, anim);
	}
	else if (animType == 0x64) {
		return ExtractorUtils::get64AnimSize(m_process, anim);
	}
	// Invalid animation type

	DEBUG_LOG("Animation address %llx does not have a valid animation type. First four bytes: %x\n", anim, m_process->readInt32(anim));

	throw;
	return 0;
}