#pragma once

#include <cstdlib>
#include <string>
#include <cstring>
#include "Structs_t7.h"
#include "GameTypes.h"

namespace T7ImportUtils
{
	bool ValidateT7Layout(const StructsT7::TKMovesetHeaderBlocks& offsets, const Byte* payload, uint64_t size);

    // Non-mutating validation of references, reachable list terminators and animation extents.
    bool ValidateT7Semantics(const StructsT7::TKMovesetHeaderBlocks& offsets, const Byte* payload,
        uint64_t size, std::string* error = nullptr);

	// Convert indexes of the displayable movelist visible in training mode
	void ConvertDisplayableMovelistOffsets(StructsT7::MvlHead* mvlHead);

	// In our locally allocated movest, correct the lists pointing to the various moveset structure lists
	void ConvertMovesetTableOffsets(const StructsT7::TKMovesetHeaderBlocks* offsets, Byte* moveset, gameAddr gameMoveset);

	//  Convert indexes of moves, cancels, hit conditions, etc... into ptrs
	bool ConvertMovesetIndexes(Byte* moveset, gameAddr gameMoveset, const StructsT7_gameAddr::MovesetTable* table, const StructsT7::TKMovesetHeaderBlocks* offsets);

	// Correct the offsets of the starting pointers of the moveset data
	// This doesn't affect gameplay and to my knowledge is not actually used
	void CorrectMovesetInfoValues(StructsT7::MovesetInfo* info, gameAddr gameMoveset);

	// Enforce the alias at the time of the extraction to be the original ones
	void EnforceCurrentAliasesAsDefault(Byte* moveset);

	// Enforce the original aliases as the current one
	void EnforceDefaultAliasesAsCurrent(Byte* moveset);
}
