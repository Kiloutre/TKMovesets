#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <fstream>

#include "LocalStorage.hpp"

enum AnimExtractionStatus_
{
	AnimExtractionStatus_NotStarted,
	AnimExtractionStatus_Started,
	AnimExtractionStatus_Skipped,
	AnimExtractionStatus_SkippedExists,
	AnimExtractionStatus_Error,
	AnimExtractionStatus_Finished,
};

struct s_extractionStatus
{
	uint32_t current_animation = 0;
	uint32_t total_animation_count = 0;
	AnimExtractionStatus_ status = AnimExtractionStatus_NotStarted;
};

struct s_animExtractionControl
{
	std::vector<s_extractionStatus> statuses;
	bool must_interrupt = false;
	unsigned int current_index = 0;
};

namespace AnimExtractor
{
	void _FromT7(const Byte* moveset, uint64_t s_moveset, const std::wstring& outputFolder, std::ofstream& outputFile, s_animExtractionControl& extractionControl);
	void _FromTTT2(const Byte* moveset, uint64_t s_moveset, const std::wstring& outputFolder, std::ofstream& outputFile, s_animExtractionControl& extractionControl);
	void _FromT6(const Byte* moveset, uint64_t s_moveset, const std::wstring& outputFolder, std::ofstream& outputFile, s_animExtractionControl& extractionControl);
	void _FromT5(const Byte* moveset, uint64_t s_moveset, const std::wstring& outputFolder, std::ofstream& outputFile, s_animExtractionControl& extractionControl);
	void _FromT5DR(const Byte* moveset, uint64_t s_moveset, const std::wstring& outputFolder, std::ofstream& outputFile, s_animExtractionControl& extractionControl);

	// Extracts the animation of every given movesets in appropriate folders
	// Does not extract if the moveset's animations have already been extracted (matches by original character name)
	void ExtractAnimations(const std::vector<movesetInfo> movesets, s_animExtractionControl& extractionControl);
};

namespace TAnimExtractorUtils
{
	// [name] comes from the moveset file: the written file and its anims.txt line use a sanitized name (AnimExtractorPaths::SafeComponent)
	void ExtractAnimation(const std::string& name, const Byte* anim, uint64_t available, const std::wstring& outputFolder, std::ofstream& outputFile, const wchar_t* extension);

	// Sizes of the [blockCount] payload blocks of a legacy moveset buffer (header + payload), whose offsets are stored in
	// order at header.block_list. Throws std::runtime_error when the header, the block table or a block lies outside the buffer.
	std::vector<uint64_t> BlockSizes(const Byte* moveset, uint64_t s_moveset, size_t blockCount);

	// String at [offset] of a block of [blockSize] bytes, or [fallback] when it lies outside the block or is not terminated in it
	std::string BlockString(const Byte* block, uint64_t blockSize, uint64_t offset, const std::string& fallback);

	// Throws std::runtime_error unless [count] records of [recordSize] bytes at [offset] fit in a block of [blockSize] bytes
	void RequireTable(uint64_t blockSize, uint64_t offset, uint64_t count, uint64_t recordSize, const char* what);

	// Bytes available to the MOTA at [offset] of the mota block (to the end of the buffer, as the extractors always used),
	// or 0 when the offset is outside the block, or the MOTA header or its animation offset list do not fit
	template<class Mota> uint64_t MotaSize(const Byte* moveset, uint64_t s_moveset, const Byte* motaBlock, uint64_t motaBlockSize, uint64_t offset)
	{
		if (offset >= motaBlockSize) {
			return 0;
		}
		const Byte* start = motaBlock + offset;
		const uint64_t used = static_cast<uint64_t>(start - moveset);
		if (used >= s_moveset) {
			return 0;
		}
		const uint64_t available = s_moveset - used;
		const uint64_t listStart = offsetof(Mota, anim_offset_list);
		if (available < listStart) {
			return 0;
		}
		const Mota& mota = *reinterpret_cast<const Mota*>(start);
		const uint32_t checkedSize = available > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32_t>(available);
		if (!mota.IsValid(checkedSize) || (available - listStart) / sizeof(mota.anim_offset_list[0]) < mota.anim_count) {
			return 0;
		}
		return available;
	}
};