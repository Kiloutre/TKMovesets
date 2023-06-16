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

	std::wstring filename = outputFolder + Helpers::to_unicode(name) + L"" ANIMATION_EXTENSION + (*anim == 0xC8 ? L"C8" : L"64");
	std::ofstream file(filename, std::ios::binary);
	file.write((char*)anim, anim_size);

	outputFile << name << "," << TAnimUtils::FromMemory::GetAnimDuration(anim) << std::endl;
}

void AnimExtractor::_FromT7(const Byte* moveset, const std::wstring& outputFolder, std::ofstream& outputFile)
{
	const TKMovesetHeader& header = *(TKMovesetHeader*)moveset;

	const TKMovesetHeaderBlocks& blocks = *(TKMovesetHeaderBlocks*)(moveset + header.block_list);

	MovesetInfo& moveset_info = *(MovesetInfo*)blocks.GetBlock(TKMovesetHeaderBlocks_MovesetInfo, moveset);
	char* name_block = (char*)blocks.GetBlock(TKMovesetHeaderBlocks_Name, moveset);
	auto moveset_block = blocks.GetBlock(TKMovesetHeaderBlocks_Moveset, moveset);
	auto animation_block = blocks.GetBlock(TKMovesetHeaderBlocks_Animation, moveset);
	auto mota_block = blocks.GetBlock(TKMovesetHeaderBlocks_Mota, moveset);


	Move* movelist = (Move*)(moveset_block + (uint64_t)moveset_info.table.move);

	std::set<void*> extracted_animations;
	for (unsigned int i = 0; i < moveset_info.table.moveCount; ++i)
	{
		auto& move = movelist[i];

		if (extracted_animations.contains(move.anim_addr))
		{
			char* anim_name = name_block + (uint64_t)move.anim_name_addr;
			const Byte* anim = animation_block + (uint64_t)move.anim_addr;
			extracted_animations.insert(move.anim_addr);
			ExtractAnimation(anim_name, anim, outputFolder, outputFile);
		}
	}
}