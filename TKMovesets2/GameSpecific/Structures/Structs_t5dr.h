#pragma once

#include "GameTypes.h"
#include "Structs_t5.h"

// Most of the names in this file indicating offsets (_0x00_int etc...) are wrong
// This is because i copy-pasted the structures from T7
// Fix it up anytime you want

namespace StructsT5DR
{
	struct MovesetInfo
	{
		char _0x0[2];
		bool isInitialized;
		char _0x3[5];
		gameAddr32 character_name_addr;
		gameAddr32 character_creator_addr;
		gameAddr32 date_addr;
		gameAddr32 fulldate_addr;
		uint32_t orig_aliases[36];
		uint32_t current_aliases[36];
		uint16_t unknown_values[36];
		char gap[8]; // unknown
		StructsT5::MovesetTable table;
		StructsT5::MotaList motas;
	};
}
