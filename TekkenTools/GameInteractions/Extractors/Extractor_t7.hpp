#pragma once

#include <string>

#include "Extractor.hpp"

#include "constants.h"
#include "Structs_t7.h"
#include "GameAddresses.h"

using namespace StructsT7;

class DLLCONTENT ExtractorT7 : public Extractor
{
private:

	// Writes bounds of the block containing anim and move names
	void getNamesBlockBounds(Move* move, uint64_t moveCount, gameAddr& start, gameAddr& end);

	// Returns an allocated block containing animations that weren't in the main animation block
	char* GetAnimations(Move* movelist, size_t moveCount, uint64_t& size_out, std::map<gameAddr, uint64_t>& offsets, std::vector<gameAddr>& animList);
	// Returns the amount of bytes an animation contains
	uint64_t GetAnimationSize(gameAddr anim, size_t maxSize);

	// Returns an allocated block containing mota block offsets followed by mota blocks
	char* allocateMotaCustomBlock(MotaList* motas, uint64_t& size_out);

public:
	using Extractor::Extractor; // Inherit constructor too
	ExtractionErrcode Extract(gameAddr playerAddress, float* progress, uint8_t gameId, bool overwriteSameFilename) override;
	bool CanExtract() override;

	std::string GetPlayerCharacterName(gameAddr playerAddress) override;
	uint32_t GetCharacterID(gameAddr playerAddress) override;


	const char* GetGameIdentifierString() override { return "T7_"; }
	const char* GetGameOriginString() override { return "Tekken 7"; }
};
