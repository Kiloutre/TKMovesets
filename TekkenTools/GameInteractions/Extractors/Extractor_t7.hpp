#pragma once

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
	void Extract(gameAddr playerAddress, float* progress) override;
	std::string GetPlayerCharacterName(gameAddr playerAddress) override;
	bool CanExtract() override;

private:
	const char* cm_gameIdentifierString = "T7_";
	const char* cm_gameOriginString = "Tekken 7";

	void getNamesBlockBounds(t7structs::Move* move, uint64_t moveCount, gameAddr& start, gameAddr& end);
};
