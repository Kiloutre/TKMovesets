#pragma once

#include <fstream>
#include <windows.h>

#include "AnimExtractors.hpp"
#include "GameIDs.hpp"

#include "constants.h"

namespace AnimExtractor
{
	void ExtractAnimations(const std::vector<movesetInfo> movesets)
	{
		DEBUG_LOG("ExtractAnimations - %llu movesets.\n", movesets.size());

		CreateDirectoryW(L"" EDITOR_LIB_DIRECTORY, nullptr);

		for (auto& m : movesets)
		{	
			DEBUG_LOG("%S\n", m.wname.c_str());
			if (!m.is_valid) continue;

			std::wstring game_prefix;
			// Don't allocate moveset if its extraction isn't supported
			switch (m.gameId)
			{
			case GameId_T7:
				game_prefix = L"T7_";
				break;
			case GameId_TTT2:
				game_prefix = L"TTT2_";
				break;
			case GameId_TREV:
				game_prefix = L"TREV_";
				break;
			case GameId_T6:
				game_prefix = L"T6_";
				break;
			default:
				continue;
			}

			const std::wstring outputFolder = L"" EDITOR_LIB_DIRECTORY "/" + game_prefix + Helpers::string_to_wstring(m.original_character) + L"/";
			const std::wstring outputFilename = outputFolder + L"anims.txt";

			if (Helpers::fileExists(outputFilename.c_str())) {
				DEBUG_LOG("File '%S' already exists: skipping extraction.\n", outputFilename.c_str());
				return;
			}

			uint64_t s_moveset;
			Byte* moveset = Helpers::ReadMovesetFile(m.filename, s_moveset);

			if (moveset == nullptr) {
				continue;
			}

			CreateDirectoryW(outputFolder.c_str(), nullptr);
			std::ofstream outputFile(outputFilename);

			switch (m.gameId)
			{
			case GameId_T7:
				_FromT7(moveset, outputFolder, outputFile);
				break;
			case GameId_TTT2:
			case GameId_TREV:
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