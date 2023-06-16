#include <fstream>

#include "AnimExtractors.hpp"
#include "Animations.hpp"

#include "Structs_t7.h"
#include "MovesetStructs.h"
#include "constants.h"

using namespace StructsT7;

void ExtractAnimation(const char* name, const Byte* anim, const std::wstring& outputFolder, std::ofstream& outputFile)
{
	auto anim_size = TAnimUtils::FromMemory::GetAnimSize(anim);

	if (anim_size == 0) {
		DEBUG_ERR("Bad anim data for anim name '%s'\n", name);
		return;
	}

	std::wstring filename = Helpers::to_unicode(name) + L"" ANIMATION_EXTENSION + (*anim == 0xC8 ? L"C8" : L"64");
	std::ofstream file(outputFolder + filename, std::ios::binary);
	if (!file.fail())
	{
		file.write((char*)anim, anim_size);
		outputFile << name << ANIMATION_EXTENSION << (*anim == 0xC8 ? "C8" : "64") << "," << TAnimUtils::FromMemory::GetAnimDuration(anim) << std::endl;
	}
}

void AnimExtractor::_FromT7(const Byte* moveset, const std::wstring& outputFolder, std::ofstream& outputFile)
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

	std::set<void*> extracted_animations;
	for (unsigned int i = 0; i < moveset_info.table.moveCount; ++i)
	{
		auto& move = movelist[i];

		if (!extracted_animations.contains(move.anim_addr))
		{
			char* anim_name = name_block + (uint64_t)move.anim_name_addr;
			const Byte* anim = animation_block + (uint64_t)move.anim_addr;
			extracted_animations.insert(move.anim_addr);
			ExtractAnimation(anim_name, anim, outputFolder, outputFile);
		}
	}

	for (unsigned int i = 0; i < _countof(moveset_info.motas.motas); ++i)
	{
		if ((gameAddr)moveset_info.motas.motas[i] == MOVESET_ADDR_MISSING) {
			continue;
		}

		std::wstring motaOutputFolder = outputFolder + L"mota_" + std::to_wstring(i) + L"/";
		CreateDirectoryW(motaOutputFolder.c_str(), nullptr);

		std::set<uint32_t> extracted_mota_animations;
		const MotaHeader& mota = *(MotaHeader*)(mota_block + (uint64_t)moveset_info.motas.motas[i]);
		std::ofstream motaOutputFile(motaOutputFolder + L"anims.txt");

		if (!mota.IsValid()) {
			continue;
		}

		for (unsigned int i = 0; i < mota.anim_count; ++i)
		{
			uint32_t anim_offset = mota.anim_offset_list[i];
			if (!extracted_mota_animations.contains(anim_offset))
			{
				extracted_mota_animations.insert(anim_offset);
				const Byte* anim = (Byte*)&mota + anim_offset;
				ExtractAnimation(std::to_string(i).c_str(), anim, motaOutputFolder, motaOutputFile);
			}
		}
	}
}