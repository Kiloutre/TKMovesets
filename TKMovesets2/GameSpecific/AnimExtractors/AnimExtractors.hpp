#pragma once

#include <string>

#include "GameIds.hpp"
#include "LocalStorage.hpp"

#include "constants.h"

namespace AnimExtractor
{
	void _FromT7(const Byte* moveset , const std::wstring& outputFolder, std::ofstream& outputFile);
	void _FromTTT2(const Byte* moveset, const std::wstring& outputFolder, std::ofstream& outputFile);
	void _FromT6(const Byte* moveset, const std::wstring& outputFolder, std::ofstream& outputFile);

	// Extracts the animation of every given movesets in appropriate folders
	// Does not extract if the moveset's animations have already been extracted (matches by original character name)
	void ExtractAnimations(const std::vector<movesetInfo> movesets)
	{
		for (auto& m : movesets)
		{	
			if (!m.is_valid) continue;

			const std::wstring outputFolder = L"" EDITOR_LIB_DIRECTORY "/" + Helpers::string_to_wstring(m.original_character);
			const std::wstring outputFilename = outputFolder + L"/anims.txt";

			if (Helpers::fileExists(outputFilename.c_str())) {
				DEBUG_LOG("File '%S' already exists: skipping extraction.\n", outputFilename.c_str());
				return;
			}

			uint64_t s_moveset;
			Byte* moveset = Helpers::ReadMovesetFile(m.filename, s_moveset);

			if (moveset == nullptr) {
				continue;
			}

			std::ofstream outputFile(outputFilename);

			switch (m.gameId)
			{
			case GameId_T7:
				_FromT7(moveset, outputFolder, outputFile);
				break;
			case GameId_TTT2:
				_FromTTT2(moveset, outputFolder, outputFile);
				break;
			case GameId_T6:
				_FromT6(moveset, outputFolder, outputFile);
				break;
			}

			free(moveset);
		}
	}
};