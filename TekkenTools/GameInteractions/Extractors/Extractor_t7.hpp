#pragma once

#include <map>
#include <string>

#include "Extractor.hpp"
#include "GameData.hpp"
#include "GameProcess.hpp"
#include "GameAddressesFile.hpp"

#include "Structs_t7.h"
#include "GameAddresses.h"

class ExtractorT7 : public Extractor
{
public:
	using Extractor::Extractor; // Inherit constructor too
	void Extract(gameAddr playerAddress, float* progress, bool overwriteSameFilename) override;
	std::string GetPlayerCharacterName(gameAddr playerAddress) override;
	bool CanExtract() override;

	const char* GetGameIdentifierString() override { return "T7_"; }
	const char* GetGameOriginString() override { return "Tekken7"; }
private:

	// Writes bounds of the block containing anim and move names
	void getNamesBlockBounds(t7structs::Move* move, uint64_t moveCount, gameAddr& start, gameAddr& end);
	// Returns an allocated block containing animations that weren't in the main animation block
	void* GetAnimations(t7structs::Move* movelist, size_t moveCount, uint64_t& size_out, std::map<gameAddr, uint64_t>& offsets);
	// Attempts to find the animation size in memory
	uint64_t TryFindAnimSize(gameAddr anim, size_t maxSize);
};
