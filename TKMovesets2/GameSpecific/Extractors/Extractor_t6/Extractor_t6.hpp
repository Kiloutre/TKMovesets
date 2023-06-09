#pragma once

#include <string>

#include "Extractor.hpp"

#include "constants.h"
#include "Structs_t6.h"
#include "MovesetStructs.h"

// This class has no storage and i would like to keep it that way.
// You should be passing values through method calls and no other way.
// It should be able to call Export() while another Export() is going on in another thread and this without breaking anything

class ExtractorT6 : public Extractor
{
private:
	/// Main functions

	// Fill the moveset table & its corresponding offset table brother.
	void FillMovesetTables(gameAddr movesetAddr, StructsT6::MovesetTable* table, StructsT6::MovesetTable* offsets);
	// Fill moveset header with our own various useful informations
	void FillHeaderInfos(TKMovesetHeader& infos, gameAddr playerAddress, uint64_t propertyCount);
	// Allocate and copy the contents of the moveset block
	Byte* CopyMovesetBlock(gameAddr movesetAddr, uint64_t& size_out, const StructsT6::MovesetTable& table, const StructsT6::MovesetTable* offsets);
	// Allocate and copy the contents of the name block
	char* CopyNameBlock(gameAddr movesetAddr, uint64_t& size_out, const StructsT6::MovesetInfo& movesetHeader);
	// Fill motalist struct, build list of anims within the mota file, allocate and copy contents of the mota block
	Byte* CopyMotaBlocks(gameAddr movesetAddr, uint64_t& size_out, StructsT6::MotaList* motasList, ExtractSettings settings);
	// Returns an allocated block containing animations that weren't in the main animation block. Also builds a map to convert anim addresses to offsets.
	Byte* CopyAnimations(const StructsT6::Move* movelist, size_t moveCount, uint64_t& size_out, std::map<gameAddr, uint64_t>& offsets);
	// Copies the moveset info block (aliases, couple strings)
	void CopyMovesetInfoBlock(gameAddr movesetAddr, StructsT6::MovesetInfo* movesetHeader);
	// Copies the displayable movelist, visible in training mode and maybe elsewhere
	Byte* CopyDisplayableMovelist(gameAddr movesetAddr, gameAddr playerAddress, uint64_t& size_out, ExtractSettings settings);

	/// Helpers

	// Returns an allocated block containing mota block offsets followed by mota blocks
	Byte* AllocateMotaCustomBlock(StructsT6::MotaList* motas, uint64_t& size_out, ExtractSettings settings);
	// Calculate the size of the mota custom block we will build, and also fill boundaries with every animation address we can find
	uint64_t CalculateMotaCustomBlockSize(const StructsT6::MotaList* motas, std::map<gameAddr32, std::pair<uint32_t, uint32_t>>& offsetMap, ExtractSettings settings);

public:
	using Extractor::Extractor; // Inherit constructor too
	ExtractionErrcode_ Extract(gameAddr playerAddress, ExtractSettings settings, uint8_t& progress) override;
	bool CanExtract() override;

	std::string GetPlayerCharacterName(gameAddr playerAddress) override;
	uint32_t GetCharacterID(gameAddr playerAddress) override;
	gameAddr GetCharacterAddress(uint8_t playerId) override;
	std::vector<gameAddr> GetCharacterAddresses() override;
};
