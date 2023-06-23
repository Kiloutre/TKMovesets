#pragma once

#include <string>

#include "LocalStorage.hpp"

enum AnimExtractionStatus_
{
	AnimExtractionStatus_NotStarted,
	AnimExtractionStatus_Started,
	AnimExtractionStatus_Skipped,
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

	// Extracts the animation of every given movesets in appropriate folders
	// Does not extract if the moveset's animations have already been extracted (matches by original character name)
	void ExtractAnimations(const std::vector<movesetInfo> movesets, s_animExtractionControl& extractionControl);
};

namespace TAnimExtractorUtils
{
	void ExtractAnimation(const char* name, const Byte* anim, const std::wstring& outputFolder, std::ofstream& outputFile, const wchar_t* extension);
};