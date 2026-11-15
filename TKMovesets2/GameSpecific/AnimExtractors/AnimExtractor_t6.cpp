#include <fstream>
#include <windows.h>

#include "AnimExtractors.hpp"

#include "Structs_t6.h"
#include "MovesetStructs.h"
#include "constants.h"

using namespace StructsT6;

void AnimExtractor::_FromT6(const Byte* moveset, uint64_t s_moveset, const std::wstring& outputFolder, std::ofstream& outputFile, s_animExtractionControl& extractionControl)
{
	const TKMovesetHeader& header = *(TKMovesetHeader*)moveset;
	const Byte* moveset_data = moveset + header.moveset_data_start;
	const TKMovesetHeaderBlocks& blocks = *(TKMovesetHeaderBlocks*)(moveset + header.block_list);
	// The file is untrusted: every block, table and MOTA walked below is bounds-checked first
	const auto blockSizes = TAnimExtractorUtils::BlockSizes(moveset, s_moveset, sizeof(blocks.blocks) / sizeof(uint64_t));
	TAnimExtractorUtils::RequireTable(blocks.nameBlock - blocks.movesetInfoBlock, 0, 1, sizeof(MovesetInfo), "Moveset info");

	MovesetInfo& moveset_info = *(MovesetInfo*)blocks.GetBlock(TKMovesetHeaderBlocks_MovesetInfo, moveset_data);
	auto moveset_block = blocks.GetBlock(TKMovesetHeaderBlocks_Moveset, moveset_data);
	auto animation_block = blocks.GetBlock(TKMovesetHeaderBlocks_Animation, moveset_data);
	auto mota_block = blocks.GetBlock(TKMovesetHeaderBlocks_Mota, moveset_data);

	TAnimExtractorUtils::RequireTable(blockSizes[TKMovesetHeaderBlocks_Moveset], (uint64_t)moveset_info.table.move, moveset_info.table.moveCount, sizeof(Move), "Move table");
	Move* movelist = (Move*)(moveset_block + (uint64_t)moveset_info.table.move);

	auto& extractionStatus = extractionControl.statuses[extractionControl.current_index];
	bool& mustInterrupt = extractionControl.must_interrupt;

	// Count unique move animations
	std::set<decltype(movelist->anim_addr)> extracted_animations;
	for (unsigned int i = 0; i < moveset_info.table.moveCount && !mustInterrupt; ++i)
	{
		auto& move = movelist[i];

		if (!extracted_animations.contains(move.anim_addr))
		{
			++extractionStatus.total_animation_count;
			extracted_animations.insert(move.anim_addr);
		}
	}

	// Count unique mota animations
	for (unsigned int i = 0; i < _countof(moveset_info.motas.motas) && !mustInterrupt; ++i)
	{
		if ((gameAddr)moveset_info.motas.motas[i] == MOVESET_ADDR32_MISSING) {
			continue;
		}

		const uint64_t mota_size = TAnimExtractorUtils::MotaSize<MotaHeader>(moveset, s_moveset, mota_block, blockSizes[TKMovesetHeaderBlocks_Mota], (uint64_t)moveset_info.motas.motas[i]);
		if (mota_size == 0) {
			continue;
		}
		const MotaHeader& mota = *(const MotaHeader*)(mota_block + (uint64_t)moveset_info.motas.motas[i]);

		std::set<uint32_t> extracted_mota_animations;
		for (unsigned int i = 0; i < mota.anim_count && !mustInterrupt; ++i)
		{
			uint32_t anim_offset = mota.anim_offset_list[i];
			if (!extracted_mota_animations.contains(anim_offset))
			{
				++extractionStatus.total_animation_count;
				extracted_mota_animations.insert(anim_offset);
			}
		}
	}

	// Extract animations
	extractionStatus.status = AnimExtractionStatus_Started;

	{
		char anim_name[32];
		std::set<decltype(movelist->anim_addr)> extracted_animations;
		for (unsigned int i = 0; i < moveset_info.table.moveCount && !mustInterrupt; ++i)
		{
			auto& move = movelist[i];

			if (!extracted_animations.contains(move.anim_addr))
			{
				sprintf_s(anim_name, sizeof(anim_name), "anim_%u", i + 1);
				if ((uint64_t)move.anim_addr >= blocks.motaBlock - blocks.animationBlock) continue;
				const Byte* anim = animation_block + move.anim_addr;
				extracted_animations.insert(move.anim_addr);
				TAnimExtractorUtils::ExtractAnimation(anim_name, anim, static_cast<uint64_t>(mota_block - anim), outputFolder, outputFile, L"" ANIMATION_EXTENSION);
				extractionStatus.current_animation++;
			}
		}
	}

	for (unsigned int i = 0; i < _countof(moveset_info.motas.motas) && !mustInterrupt; ++i)
	{
		if (moveset_info.motas.motas[i] == MOVESET_ADDR32_MISSING) {
			continue;
		}

		std::wstring motaOutputFolder = outputFolder + L"mota_" + std::to_wstring(i) + L"/";
		CreateDirectoryW(motaOutputFolder.c_str(), nullptr);

		std::set<uint32_t> extracted_mota_animations;
		std::ofstream motaOutputFile(motaOutputFolder + L"anims.txt");

		const uint64_t mota_size = TAnimExtractorUtils::MotaSize<MotaHeader>(moveset, s_moveset, mota_block, blockSizes[TKMovesetHeaderBlocks_Mota], (uint64_t)moveset_info.motas.motas[i]);
		if (mota_size == 0) {
			continue;
		}
		const MotaHeader& mota = *(const MotaHeader*)(mota_block + (uint64_t)moveset_info.motas.motas[i]);

		const wchar_t* extension;

		switch (i)
		{
		case 2:
		case 3:
			extension = L"" ANIMATION_HAND_EXTENSION;
			break;
		case 4:
		case 5:
			extension = L"" ANIMATION_FACE_EXTENSION;
			break;
		case 8:
		case 9:
			extension = L"" ANIMATION_CAM_EXTENSION;
			break;
		default:
			extension = L"" ANIMATION_OTHER_EXTENSION;
			break;
		}

		for (unsigned int i = 0; i < mota.anim_count && !mustInterrupt; ++i)
		{
			uint32_t anim_offset = mota.anim_offset_list[i];
			if (!extracted_mota_animations.contains(anim_offset))
			{
				extracted_mota_animations.insert(anim_offset);
				if (anim_offset >= mota_size) continue;
				const Byte* anim = (Byte*)&mota + anim_offset;
				TAnimExtractorUtils::ExtractAnimation(std::to_string(i).c_str(), anim, mota_size - anim_offset, motaOutputFolder, motaOutputFile, extension);
				extractionStatus.current_animation++;
			}
		}
	}

	extractionStatus.status = AnimExtractionStatus_Finished;
}