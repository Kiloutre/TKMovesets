#pragma once

#include <filesystem>
#include <fstream>
#include <format>
#include <windows.h>

#include "AnimExtractors.hpp"
#include "GameIDs.hpp"
#include "Animations.hpp"
#include "Compression.hpp"

#include "constants.h"
#include "MovesetStructs.h"

namespace AnimExtractor
{
	void ExtractAnimations(const std::vector<movesetInfo> movesets, s_animExtractionControl& extractionControl)
	{
		DEBUG_LOG("ExtractAnimations - %llu movesets.\n", movesets.size());

		CreateDirectoryW(L"" EDITOR_LIB_DIRECTORY, nullptr);


		for (unsigned int i = 0; i < movesets.size() && !extractionControl.must_interrupt; ++i, extractionControl.current_index++)
		{	
			auto& m = movesets[i];
			auto& extractionStatus = extractionControl.statuses[i];

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
			const std::wstring outputFilename = outputFolder + L"anims_tmp.txt";
			const std::wstring outputFinalFilename = outputFolder + L"anims.txt";

			{
				struct _stat animCacheFile;
				if (_wstat(outputFinalFilename.c_str(), &animCacheFile) == 0 && animCacheFile.st_size != 0) {
					DEBUG_LOG("File '%S' already exists (and is not empty): skipping extraction.\n", outputFinalFilename.c_str());
					extractionStatus.status = AnimExtractionStatus_Skipped;
					continue;
				}
			}

			uint64_t s_moveset;
			Byte* moveset;
			TKMovesetHeader* movesetHeader;
            
            try {
                moveset = Helpers::ReadMovesetFile(m.filename, s_moveset);
				movesetHeader = (TKMovesetHeader*)moveset;
            } catch(const std::exception&)
            {
				extractionStatus.status = AnimExtractionStatus_Error;
				DEBUG_ERR("'%S': Moveset error", m.filename);
				continue;
            }

			if (movesetHeader->conversion_origin != 0)
			{
				// Skip converted movesets
				extractionStatus.status = AnimExtractionStatus_Skipped;
				DEBUG_LOG("'%S': Converted moveset: skipping\n", m.filename);
			}
			else
			{

				if (movesetHeader->isCompressed())
				{
					Byte* orig_moveset = moveset;

					int32_t compressed_data_size = (int32_t)(s_moveset - movesetHeader->moveset_data_start);
					moveset = CompressionUtils::RAW::Moveset::DecompressWithHeader(moveset, compressed_data_size, s_moveset);

					delete[] orig_moveset;

					if (moveset == nullptr) {
						DEBUG_ERR("Failed decompress moveset '%S''", m.filename);
						extractionStatus.status = AnimExtractionStatus_Error;
						continue;
					}
				}

				CreateDirectoryW(outputFolder.c_str(), nullptr);
				std::ofstream outputFile(outputFilename);

				switch (m.gameId)
				{
				case GameId_T7:
					_FromT7(moveset, s_moveset, outputFolder, outputFile, extractionControl);
					break;
				case GameId_TTT2:
				case GameId_TREV:
					_FromTTT2(moveset, s_moveset, outputFolder, outputFile, extractionControl);
					break;
				case GameId_T6:
					_FromT6(moveset, s_moveset, outputFolder, outputFile, extractionControl);
					break;
				case GameId_T5:
					_FromT5(moveset, s_moveset, outputFolder, outputFile, extractionControl);
					break;
				}

				outputFile.close();

				if (!extractionControl.must_interrupt) {
					try {
						std::filesystem::remove(outputFinalFilename);
					}
					catch (std::filesystem::filesystem_error const&) {
					}

					try {
						std::filesystem::rename(outputFilename, outputFinalFilename);
					}
					catch (const std::exception&) {
						DEBUG_ERR("Failed to rename '%S' to '%S'", outputFilename.c_str(), outputFinalFilename.c_str());
						extractionStatus.status = AnimExtractionStatus_Error;
					}
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

			// Animation Name
			outputFile << name << Helpers::wstring_to_string(extension) << (*anim == 0xC8 ? "C8" : "64");
			outputFile << ",";
			// Animation duration
			outputFile << TAnimUtils::FromMemory::GetAnimDuration(anim);
			outputFile << ",";
			// Animation size
			outputFile << anim_size;
			outputFile << ",";
			// Set up HEX formatting
			// Animation hash
			uint32_t crc32 = Helpers::CalculateCrc32(anim, anim_size);
			outputFile << std::format("{:08X}", crc32) << std::endl;
		}
	}
}