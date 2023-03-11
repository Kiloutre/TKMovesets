#pragma once

#include <string>

#include "Importer.hpp"

#include "constants.h"
#include "Structs_t7.h"
#include "MovesetStructs.h"

using namespace StructsT7;

// This class has no storage and i would like to keep it that way.
// You should be passing values through method calls and no other way.
// It should be able to call Import() while another Import() is going on in another thread and this without breaking anything

class DLLCONTENT ImporterT7 : public Importer
{
private:
	// In our locally allocated moveset, correct the mota lists offsets or copy the motas from the currently loaded character if we can't provide them
	void ConvertMotaListOffsets(const TKMovesetHeader_offsets& offsets, Byte* moveset, gameAddr gameMoveset, gameAddr playerAddress);
	// In our locally allocated movest, correct the lists pointing to the various moveset structure lists
	void ConvertMovesetTableOffsets(const TKMovesetHeader_offsets& offsets, Byte* moveset, gameAddr gameMoveset);
	//  Convert indexes of moves, cancels, hit conditions, etc... into ptrs
	void ConvertMovesetIndexes(Byte* moveset, gameAddr gameMoveset, const StructsT7_gameAddr::MovesetTable* offsets, const TKMovesetHeader_offsets& blockOffsets);

	// Write the player's camera motas to his structure. This is required for proper camera mota importation, and does not break anything of those camera mota have not been imported by us
	void WriteCameraMotasToPlayer(gameAddr movesetAddr, gameAddr playerAddress);
	// Fixes move that rely on correct character IDs to work
	void ApplyCharacterIDFixes(Byte* moveset, gameAddr playerAddress, const StructsT7_gameAddr::MovesetTable* offsets, const TKMovesetHeader& header);

public:
	using Importer::Importer; // Inherit constructor too
	ImportationErrcode_ Import(const Byte* orig_moveset, uint64_t s_moveset, gameAddr playerAddress, bool applyInstantly, uint8_t& progress) override;
	bool CanImport() override;
	void CleanupUnusedMovesets() override;

	// Forces the new moveset on the player and play a specific move
	void ForcePlayerMove(gameAddr playerAddress, gameAddr playerMoveset, size_t moveId);

	gameAddr GetCharacterAddress(uint8_t playerId) override;
	gameAddr GetMovesetAddress(uint8_t playerId) override;
	void SetCurrentMove(gameAddr playerAddress, gameAddr playerMoveset, size_t moveId) override;
};
