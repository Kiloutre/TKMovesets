#include <fstream>
#include <windows.h>

#include "AnimExtractors.hpp"
#include "Animations.hpp"

#include "Structs_t5dr.h"
#include "MovesetStructs.h"
#include "constants.h"

using namespace StructsT5;

#define MovesetInfo StructsT5DR::MovesetInfo

void AnimExtractor::_FromT5DR(const Byte* moveset, uint64_t s_moveset, const std::wstring& outputFolder, std::ofstream& outputFile, s_animExtractionControl& extractionControl)
{
	const TKMovesetHeader& header = *(TKMovesetHeader*)moveset;
	const Byte* moveset_data = moveset + header.moveset_data_start;
	const TKMovesetHeaderBlocks& blocks = *(TKMovesetHeaderBlocks*)(moveset + header.block_list);

	MovesetInfo& moveset_info = *(MovesetInfo*)blocks.GetBlock(TKMovesetHeaderBlocks_MovesetInfo, moveset_data);
	char* name_block = (char*)blocks.GetBlock(TKMovesetHeaderBlocks_Name, moveset_data);
	auto moveset_block = blocks.GetBlock(TKMovesetHeaderBlocks_Moveset, moveset_data);
	auto animation_block = blocks.GetBlock(TKMovesetHeaderBlocks_Animation, moveset_data);
	auto mota_block = blocks.GetBlock(TKMovesetHeaderBlocks_Mota, moveset_data);

	Move* movelist = (Move*)(moveset_block + (uint64_t)moveset_info.table.move);

	auto& extractionStatus = extractionControl.statuses[extractionControl.current_index];
	bool& mustInterrupt = extractionControl.must_interrupt;

	// Get animation count
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

		const MotaHeader& mota = *(MotaHeader*)(mota_block + (uint64_t)moveset_info.motas.motas[i]);

		uint32_t mota_size = (uint32_t)(s_moveset - (((Byte*)&mota) - moveset));

		if (!mota.IsValid(mota_size)) {
			continue;
		}

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
		std::set<decltype(movelist->anim_addr)> extracted_animations;
		for (unsigned int i = 0; i < moveset_info.table.moveCount && !mustInterrupt; ++i)
		{
			auto& move = movelist[i];

			if (!extracted_animations.contains(move.anim_addr))
			{
				char* anim_name = name_block + (uint64_t)move.anim_name_addr;
				const Byte* anim = animation_block + (uint64_t)move.anim_addr;
				extracted_animations.insert(move.anim_addr);
				TAnimExtractorUtils::ExtractAnimation(anim_name, anim, outputFolder, outputFile, L"" ANIMATION_EXTENSION);
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
		const MotaHeader& mota = *(MotaHeader*)(mota_block + (uint64_t)moveset_info.motas.motas[i]);
		std::ofstream motaOutputFile(motaOutputFolder + L"anims.txt");

		uint32_t mota_size = (uint32_t)(s_moveset - (((Byte*)&mota) - moveset));

		if (!mota.IsValid(mota_size)) {
			continue;
		}

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
				const Byte* anim = (Byte*)&mota + anim_offset;
				TAnimExtractorUtils::ExtractAnimation(std::to_string(i).c_str(), anim, motaOutputFolder, motaOutputFile, extension);
				extractionStatus.current_animation++;
			}
		}
	}

	extractionStatus.status = AnimExtractionStatus_Finished;
}