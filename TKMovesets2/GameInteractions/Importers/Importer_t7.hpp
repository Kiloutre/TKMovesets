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
	void ConvertMotaListOffsets(const MovesetHeader_offsets& offsets, char* movesetData, gameAddr gameMoveset, gameAddr playerAddress);
	// In our locally allocated movest, correct the lists pointing to the various moveset structure lists
	void ConvertMovesetTableOffsets(const MovesetHeader_offsets& offsets, char* movesetData, gameAddr gameMoveset);
	//  Convert indexes of moves, cancels, hit conditions, etc... into ptrs
	void ConvertMovesetIndexes(char* movesetData, gameAddr gameMoveset, const StructsT7_gameAddr::MovesetTable* offsets, const MovesetHeader_offsets& blockOffsets);
	// Write the player's camera motas to his structure. This is required for proper camera mota importation, and does not break anything of those camera mota have not been imported by us
	void WriteCameraMotasToPlayer(gameAddr movesetAddr, gameAddr playerAddress);

	// Sets the move of the player
	void SetCurrentMove(gameAddr playerAddress, gameAddr playerMoveset, size_t moveId);
public:
	using Importer::Importer; // Inherit constructor too
	ImportationErrcode Import(const char* filename, gameAddr playerAddress, bool applyInstantly, float& progress) override;
	bool CanImport() override;
	void CleanupUnusedMovesets() override;
};
