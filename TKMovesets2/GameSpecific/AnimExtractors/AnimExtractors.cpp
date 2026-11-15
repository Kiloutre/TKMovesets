#pragma once

#include <filesystem>
#include <fstream>
#include <format>
#include <stdexcept>
#include <cstring>
#include <vector>
#include <windows.h>

#include "AnimExtractors.hpp"
#include "GameIDs.hpp"
#include "Animations.hpp"
#include "Compression.hpp"
#include "AnimExtractor_t8.hpp"
#include "MovesetFile.hpp"

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
			if(m.gameId==GameId_T8 && m.minorVersion==GameVersions::T8::RETAIL) {
                extractionStatus.status=AnimExtractionStatus_Started;
                try {
                    MovesetFile file(m.filename);file.Validate();
                    auto base=std::filesystem::absolute(EDITOR_LIB_DIRECTORY);
                    auto stem=std::filesystem::path(m.filename).stem().wstring()+L"_"+std::to_wstring(Helpers::getCurrentTimestamp());
                    auto directory=base/stem;
                    for(unsigned suffix=2;std::filesystem::exists(directory);++suffix)directory=base/(stem+L"_"+std::to_wstring(suffix));
                    RetailT8Animations::Extract(file.Payload(),directory,&extractionControl.must_interrupt,
                        &extractionStatus.current_animation,&extractionStatus.total_animation_count);
                    extractionStatus.status=extractionControl.must_interrupt?AnimExtractionStatus_NotStarted:AnimExtractionStatus_Finished;
                }catch(const std::exception&){extractionStatus.status=AnimExtractionStatus_Error;}
                continue;
            }
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

			// The character name comes from the file header: one safe folder name under the editor library (no "..", separators...)
			const std::wstring outputFolder = L"" EDITOR_LIB_DIRECTORY "/" + game_prefix + Helpers::string_to_wstring(AnimExtractorPaths::SafeComponent(m.original_character)) + L"/";
			const std::wstring outputFilename = outputFolder + L"anims_tmp.txt";
			const std::wstring outputFinalFilename = outputFolder + L"anims.txt";

			{
				struct _stat animCacheFile;
				if (_wstat(outputFinalFilename.c_str(), &animCacheFile) == 0 && animCacheFile.st_size != 0) {
					DEBUG_LOG("File '%S' already exists (and is not empty): skipping extraction.\n", outputFinalFilename.c_str());
					extractionStatus.status = AnimExtractionStatus_SkippedExists;
					continue;
				}
			}

			try {
				// Reads and decompresses the file (header checks only)
				MovesetFile file(m.filename);

				if (file.Header().conversion_origin != 0)
				{
					// Skip converted movesets
					extractionStatus.status = AnimExtractionStatus_Skipped;
					DEBUG_LOG("'%S': Converted moveset: skipping\n", m.filename.c_str());
					continue;
				}

				// The file is untrusted: same record validation as conversion and import before any table is walked
				file.Validate();
				const Byte* moveset = file.Bytes().data();
				const uint64_t s_moveset = file.Bytes().size();

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
					if (m.minorVersion == GameVersions::T5::DR_ONLINE) {
						_FromT5DR(moveset, s_moveset, outputFolder, outputFile, extractionControl);
					}
					else {
						_FromT5(moveset, s_moveset, outputFolder, outputFile, extractionControl);
					}
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
			catch (const std::exception&)
			{
				// Invalid or unreadable moveset, or a table outside the file: this moveset is skipped with an error
				extractionStatus.status = AnimExtractionStatus_Error;
				DEBUG_ERR("'%S': Moveset error", m.filename.c_str());
			}
		}
	}
};

namespace TAnimExtractorUtils
{
	std::vector<uint64_t> BlockSizes(const Byte* moveset, uint64_t s_moveset, size_t blockCount)
	{
		if (s_moveset < sizeof(TKMovesetHeader)) {
			throw std::runtime_error("Moveset header is truncated");
		}
		const TKMovesetHeader& header = *(const TKMovesetHeader*)moveset;
		const uint64_t dataStart = header.moveset_data_start;
		if (header.block_list_size < blockCount || header.block_list > s_moveset ||
			blockCount * sizeof(uint64_t) > s_moveset - header.block_list || dataStart > s_moveset) {
			throw std::runtime_error("Moveset block table outside the file");
		}
		const uint64_t payloadSize = s_moveset - dataStart;
		std::vector<uint64_t> offsets(blockCount);
		memcpy(offsets.data(), moveset + header.block_list, blockCount * sizeof(uint64_t));
		std::vector<uint64_t> sizes(blockCount);
		for (size_t i = 0; i < blockCount; ++i) {
			const uint64_t end = i + 1 < blockCount ? offsets[i + 1] : payloadSize;
			if (offsets[i] > end || end > payloadSize) {
				throw std::runtime_error("Moveset block outside the payload");
			}
			sizes[i] = end - offsets[i];
		}
		return sizes;
	}

	std::string BlockString(const Byte* block, uint64_t blockSize, uint64_t offset, const std::string& fallback)
	{
		if (offset >= blockSize) {
			return fallback;
		}
		const char* start = (const char*)block + offset;
		const void* end = memchr(start, 0, static_cast<size_t>(blockSize - offset));
		return end ? std::string(start, (const char*)end) : fallback;
	}

	void RequireTable(uint64_t blockSize, uint64_t offset, uint64_t count, uint64_t recordSize, const char* what)
	{
		if (offset > blockSize || (recordSize != 0 && count > (blockSize - offset) / recordSize)) {
			throw std::runtime_error(std::string(what) + " outside its block");
		}
	}

	void ExtractAnimation(const std::string& rawName, const Byte* anim, uint64_t available, const std::wstring& outputFolder, std::ofstream& outputFile, const wchar_t* extension)
	{
		auto anim_size = TAnimUtils::FromMemory::Safe::GetAnimSize(anim, available);

		if (anim_size == 0) {
			DEBUG_ERR("Bad anim data for anim name '%s'\n", rawName.c_str());
			return;
		}

		// The name comes from the file: never a path ("..\", separators), a reserved name or invalid UTF-8 (to_unicode throws)
		const std::string name = AnimExtractorPaths::SafeComponent(rawName);
		std::wstring filename;
		try {
			filename = Helpers::to_unicode(name) + extension + (*anim == 0xC8 ? L"C8" : L"64");
		}
		catch (const std::exception&) {
			DEBUG_ERR("Bad anim name '%s'\n", name.c_str());
			return;
		}
		std::ofstream file(outputFolder + filename, std::ios::binary);
		if (!file.fail())
		{
			file.write((char*)anim, anim_size);

			// Animation Name
			outputFile << name << Helpers::wstring_to_string(extension) << (*anim == 0xC8 ? "C8" : "64");
			outputFile << ",";
			// Animation duration
			outputFile << TAnimUtils::FromMemory::Safe::GetAnimDuration(anim, available);
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
