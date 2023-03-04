#pragma once

#include <string>

#include "Extractor.hpp"

#include "constants.h"
#include "Structs_t7.h"
#include "MovesetStructs.h"

using namespace StructsT7;

// This class has no storage and i would like to keep it that way. You should be passing values through method calls and no other way.
class DLLCONTENT ExtractorT7 : public Extractor
{
private:
	/// Main functions

	// Fill the moveset table & its corresponding offset table brother.
	void FillMovesetTables(gameAddr movesetAddr, StructsT7_gameAddr::MovesetTable* table, StructsT7_gameAddr::MovesetTable* offsets);
	// Fill moveset header with our own various useful informations
	void FillHeaderInfos(MovesetHeader_infos& infos, uint8_t gameId, gameAddr playerAddress);
	// Allocate and copy the contents of the moveset block
	char* CopyMovesetBlock(gameAddr movesetAddr, uint64_t& size_out, StructsT7_gameAddr::MovesetTable& table);
	// Allocate and copy the contents of the name block
	char* CopyNameBlock(gameAddr movesetAddr, uint64_t& size_out, Move* movelist, uint64_t moveCount, gameAddr& nameBlockStart);
	// Fill motalist struct, build list of anims within the mota file, allocate and copy contents of the mota block
	char* CopyMotaBlocks(gameAddr movesetAddr, uint64_t& size_out, MotaList* motasList, std::vector<gameAddr>& animBoundaries);
	// Returns an allocated block containing animations that weren't in the main animation block. Also builds a map to convert anim addresses to offsets.
	char* CopyAnimations(Move* movelist, size_t moveCount, uint64_t& size_out, std::map<gameAddr, uint64_t>& offsets, std::vector<gameAddr>& animList);

	/// Helpers

	// Writes bounds of the block containing anim and move names
	void getNamesBlockBounds(Move* move, uint64_t moveCount, gameAddr& start, gameAddr& end);
	// Returns the amount of bytes an animation contains
	uint64_t GetAnimationSize(gameAddr anim, size_t maxSize);
	// Returns an allocated block containing mota block offsets followed by mota blocks
	char* allocateMotaCustomBlock(MotaList* motas, uint64_t& size_out);

public:
	using Extractor::Extractor; // Inherit constructor too
	ExtractionErrcode Extract(gameAddr playerAddress, uint8_t gameId, bool overwriteSameFilename, float& progress) override;
	bool CanExtract() override;

	std::string GetPlayerCharacterName(gameAddr playerAddress) override;
	uint32_t GetCharacterID(gameAddr playerAddress) override;


	const char* GetGameIdentifierString() override { return "T7_"; }
	const char* GetGameOriginString() override { return "Tekken 7"; }
};
