#pragma once

#include <string>

#include "Importer.hpp"

#include "Structs_t7.h"

// This class has no storage and i would like to keep it that way.
// You should be passing values through method calls and no other way.
// It should be able to call Import() while another Import() is going on in another thread and this without breaking anything

class ImporterT7 : public Importer
{
private:
	// In our locally allocated moveset, correct the mota lists offsets or copy the motas from the currently loaded character if we can't provide them
	void ConvertMotaListOffsets(const StructsT7::TKMovesetHeaderBlocks* offsets, Byte* moveset, gameAddr gameMoveset, gameAddr playerAddress, bool replaceEmpty);

	// Write the player's camera motas to his structure. This is required for proper camera mota importation, and does not break anything of those camera mota have not been imported by us
	void WriteCameraMotasToPlayer(gameAddr movesetAddr, gameAddr playerAddress);
	// Fixes move that rely on correct character IDs to work
	void ApplyCharacterIDFixes(Byte* moveset, gameAddr playerAddress, const StructsT7_gameAddr::MovesetTable* table, const TKMovesetHeader* header, const StructsT7::TKMovesetHeaderBlocks* blockOoffsetsffsets);
	// Import the displayable movelist
	void ImportMovelist(StructsT7::MvlHead* mvlHead, gameAddr game_mlvHead, gameAddr playerAddress);

	ImportationErrcode_ ImportMovesetData(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress) override;

	ImportationErrcode_ _Import_FromT7(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress);
public:
	using Importer::Importer; // Inherit constructor too
	bool CanImport() override;
	void CleanupUnusedMovesets() override;

	// Forces the new moveset on the player and play a specific move
	void ForcePlayerMove(gameAddr playerAddress, gameAddr playerMoveset, size_t moveId);

	gameAddr GetCharacterAddress(uint8_t playerId) override;
	gameAddr GetMovesetAddress(uint8_t playerId) override;
};
