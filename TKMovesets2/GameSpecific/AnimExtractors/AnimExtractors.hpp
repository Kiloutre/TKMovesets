#pragma once

#include <string>

#include "LocalStorage.hpp"

enum AnimExtractionStatus_
{
	AnimExtractionStatus_NotStarted,
	AnimExtractionStatus_Started,
	AnimExtractionStatus_Skipped,
	AnimExtractionStatus_Finished,
};

struct s_extractionStatus
{
	uint32_t current_animation = 0;
	uint32_t total_animation_count = 0;
	AnimExtractionStatus_ status = AnimExtractionStatus_NotStarted;
};

namespace AnimExtractor
{
	void _FromT7(const Byte* moveset, uint64_t s_moveset, const std::wstring& outputFolder, std::ofstream& outputFile, s_extractionStatus& extractionStatus);
	void _FromTTT2(const Byte* moveset, uint64_t s_moveset, const std::wstring& outputFolder, std::ofstream& outputFile, s_extractionStatus& extractionStatus);
	void _FromT6(const Byte* moveset, uint64_t s_moveset, const std::wstring& outputFolder, std::ofstream& outputFile, s_extractionStatus& extractionStatus);
	void _FromT5(const Byte* moveset, uint64_t s_moveset, const std::wstring& outputFolder, std::ofstream& outputFile, s_extractionStatus& extractionStatus);

	// Extracts the animation of every given movesets in appropriate folders
	// Does not extract if the moveset's animations have already been extracted (matches by original character name)
	void ExtractAnimations(const std::vector<movesetInfo> movesets, std::vector<s_extractionStatus>& extractionStatuses);
};

namespace TAnimExtractorUtils
{
	void ExtractAnimation(const char* name, const Byte* anim, const std::wstring& outputFolder, std::ofstream& outputFile, const wchar_t* extension);
};