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
	bool CanExtract() override;

	std::string GetPlayerCharacterName(gameAddr playerAddress) override;
	uint32_t GetCharacterID(gameAddr playerAddress) override;


	const char* GetGameIdentifierString() override { return "T7_"; }
	const char* GetGameOriginString() override { return "Tekken 7"; }
private:

	// Writes bounds of the block containing anim and move names
	void getNamesBlockBounds(t7structs::Move* move, uint64_t moveCount, gameAddr& start, gameAddr& end);

	// Returns an allocated block containing animations that weren't in the main animation block
	void* GetAnimations(t7structs::Move* movelist, size_t moveCount, uint64_t& size_out, std::map<gameAddr, uint64_t>& offsets, std::vector<gameAddr>& animList);
	// Attempts to find the animation size in memory
	uint64_t TryFindAnimSize(gameAddr anim, size_t maxSize);
};
