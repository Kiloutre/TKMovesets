#pragma once

#include <string>

#include "LocalStorage.hpp"

namespace AnimExtractor
{
	void _FromT7(const Byte* moveset , const std::wstring& outputFolder, std::ofstream& outputFile);
	void _FromTTT2(const Byte* moveset, const std::wstring& outputFolder, std::ofstream& outputFile);
	void _FromT6(const Byte* moveset, const std::wstring& outputFolder, std::ofstream& outputFile);

	// Extracts the animation of every given movesets in appropriate folders
	// Does not extract if the moveset's animations have already been extracted (matches by original character name)
	void ExtractAnimations(const std::vector<movesetInfo> movesets);
};