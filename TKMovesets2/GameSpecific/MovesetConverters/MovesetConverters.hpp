#pragma once

#include "constants.h"
#include "MovesetStructs.h"
#include "GameTypes.h"

#include "Structs_t7.h"

namespace MovesetConverter
{
	void TTT2ToT7(const TKMovesetHeader* header, Byte*& moveset, uint64_t& s_moveset, StructsT7::TKMovesetHeaderBlocks& blocks_out);
};