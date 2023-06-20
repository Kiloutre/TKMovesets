#pragma once

#include <fstream>
#include <windows.h>

#include "AnimExtractors.hpp"
#include "GameIDs.hpp"
#include "Animations.hpp"

#include "constants.h"
#include "MovesetStructs.h"

namespace AnimExtractor
{
	void ExtractAnimations(const std::vector<movesetInfo> movesets, std::vector<s_extractionStatus>& extractionStatuses)
	{
		DEBUG_LOG("ExtractAnimations - %llu movesets.\n", movesets.size());

		CreateDirectoryW(L"" EDITOR_LIB_DIRECTORY, nullptr);

		for (unsigned int i = 0; i < movesets.size(); ++i)
		{	
			auto& m = movesets[i];
			auto& extractionStatus = extractionStatuses[i];

			DEBUG_LOG("%S\n", m.wname.c_str());
			if (!m.is_valid) {
				extractionStatus.status = AnimExtractionStatus_Skipped;
				continue;
			}

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
			case GameId_T5:
				game_prefix = L"T5_";
				break;
			default:
				extractionStatus.status = AnimExtractionStatus_Skipped;
				continue;
			}

			const std::wstring outputFolder = L"" EDITOR_LIB_DIRECTORY "/" + game_prefix + Helpers::string_to_wstring(m.original_character) + L"/";
			const std::wstring outputFilename = outputFolder + L"anims.txt";

			if (Helpers::fileExists(outputFilename.c_str())) {
				DEBUG_LOG("File '%S' already exists: skipping extraction.\n", outputFilename.c_str());
				extractionStatus.status = AnimExtractionStatus_Skipped;
				continue;
			}

			uint64_t s_moveset;
			Byte* moveset;
            
            try {
                moveset = Helpers::ReadMovesetFile(m.filename, s_moveset);
            } catch(const std::exception&)
            {
				extractionStatus.status = AnimExtractionStatus_Skipped;
				continue;
            }

			if (((TKMovesetHeader*)moveset)->conversion_origin != 0)
			{
				// Skip converted movesets
				extractionStatus.status = AnimExtractionStatus_Skipped;
			}
			else
			{
				CreateDirectoryW(outputFolder.c_str(), nullptr);
				std::ofstream outputFile(outputFilename);

				switch (m.gameId)
				{
				case GameId_T7:
					_FromT7(moveset, s_moveset, outputFolder, outputFile, extractionStatus);
					break;
				case GameId_TTT2:
				case GameId_TREV:
					_FromTTT2(moveset, s_moveset, outputFolder, outputFile, extractionStatus);
					break;
				case GameId_T6:
					_FromT6(moveset, s_moveset, outputFolder, outputFile, extractionStatus);
					break;
				case GameId_T5:
					_FromT5(moveset, s_moveset, outputFolder, outputFile, extractionStatus);
					break;
				}
			}

			free(moveset);
		}
	}
};

namespace TAnimExtractorUtils
{
	void ExtractAnimation(const char* name, const Byte* anim, const std::wstring& outputFolder, std::ofstream& outputFile, const wchar_t* extension)
	{
		auto anim_size = TAnimUtils::FromMemory::GetAnimSize(anim);

		if (anim_size == 0) {
			DEBUG_ERR("Bad anim data for anim name '%s'\n", name);
			return;
		}

		std::wstring filename = Helpers::to_unicode(name) + extension + (*anim == 0xC8 ? L"C8" : L"64");
		std::ofstream file(outputFolder + filename, std::ios::binary);
		if (!file.fail())
		{
			file.write((char*)anim, anim_size);
			outputFile << name << Helpers::wstring_to_string(extension) << (*anim == 0xC8 ? "C8" : "64") << "," << TAnimUtils::FromMemory::GetAnimDuration(anim) << "," << anim_size << std::endl;
		}
	}
}