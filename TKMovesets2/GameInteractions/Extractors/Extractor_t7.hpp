#pragma once

#include <string>

#include "Extractor.hpp"

#include "constants.h"
#include "Structs_t7.h"
#include "MovesetStructs.h"

using namespace StructsT7;

// This class has no storage and i would like to keep it that way.
// You should be passing values through method calls and no other way.
// It should be able to call Export() while another Export() is going on in another thread and this without breaking anything

class DLLCONTENT ExtractorT7 : public Extractor
{
private:
	/// Main functions

	// Fill the moveset table & its corresponding offset table brother.
	void FillMovesetTables(gameAddr movesetAddr, StructsT7_gameAddr::MovesetTable* table, StructsT7_gameAddr::MovesetTable* offsets);
	// Fill moveset header with our own various useful informations
	void FillHeaderInfos(TKMovesetHeader_infos& infos, uint8_t gameId, gameAddr playerAddress);
	// Allocate and copy the contents of the moveset block
	Byte* CopyMovesetBlock(gameAddr movesetAddr, uint64_t& size_out, const StructsT7_gameAddr::MovesetTable& table);
	// Allocate and copy the contents of the name block
	char* CopyNameBlock(gameAddr movesetAddr, uint64_t& size_out, const StructsT7_gameAddr::Move* movelist, uint64_t moveCount, gameAddr& nameBlockStart);
	// Fill motalist struct, build list of anims within the mota file, allocate and copy contents of the mota block
	Byte* CopyMotaBlocks(gameAddr movesetAddr, uint64_t& size_out, MotaList* motasList, std::vector<gameAddr>& boundaries, ExtractSettings settings);
	// Returns an allocated block containing animations that weren't in the main animation block. Also builds a map to convert anim addresses to offsets.
	Byte* CopyAnimations(const StructsT7_gameAddr::Move* movelist, size_t moveCount, uint64_t& size_out, std::map<gameAddr, uint64_t>& offsets, std::vector<gameAddr>& boundaries);

	/// Helpers

	// Writes bounds of the block containing anim and move names
	void GetNamesBlockBounds(const StructsT7_gameAddr::Move* move, uint64_t moveCount, gameAddr& start, gameAddr& end);
	// Returns the amount of bytes an animation contains
	uint64_t GetAnimationSize(gameAddr anim, size_t maxSize);
	// Returns an allocated block containing mota block offsets followed by mota blocks
	Byte* AllocateMotaCustomBlock(MotaList* motas, uint64_t& size_out, std::vector<gameAddr>& boundaries, ExtractSettings settings);
	// Calculate the size of the mota custom block we will build, and also fill boundaries with every animation address we can find
	uint64_t CalculateMotaCustomBlockSize(const MotaList* motas, std::vector<gameAddr>& boundaries, std::map<gameAddr, uint64_t>& offsetMap, ExtractSettings settings);


	const char* GetGameIdentifierString() override { return "T7_"; }
	const char* GetGameOriginString() override { return "Tekken 7"; }

public:
	using Extractor::Extractor; // Inherit constructor too
	ExtractionErrcode_ Extract(gameAddr playerAddress, ExtractSettings settings, uint8_t gameId, uint8_t& progress) override;
	bool CanExtract() override;

	std::string GetPlayerCharacterName(gameAddr playerAddress) override;
	uint32_t GetCharacterID(gameAddr playerAddress) override;
	gameAddr GetCharacterAddress(uint8_t playerId) override;
	std::vector<gameAddr> GetCharacterAddresses() override;
};
