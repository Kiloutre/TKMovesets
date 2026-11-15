#pragma once

#include <string>

#include "Extractor.hpp"

#include "constants.h"
#include "Structs_t7.h"
#include "MovesetStructs.h"

// This class has no storage and i would like to keep it that way.
// You should be passing values through method calls and no other way.
// It should be able to call Export() while another Export() is going on in another thread and this without breaking anything

class ExtractorT7 : public Extractor
{
private:
    void FillHeaderInfos(TKMovesetHeader& infos, gameAddr playerAddress, const std::string& characterName);

public:
	using Extractor::Extractor; // Inherit constructor too
	ExtractionErrcode_ Extract(gameAddr playerAddress, ExtractSettings settings, uint8_t& progress) override;
	bool CanExtract() override;

	std::string GetPlayerCharacterName(gameAddr playerAddress) override;
	uint32_t GetCharacterID(gameAddr playerAddress) override;
	gameAddr GetCharacterAddress(uint8_t playerId) override;
	std::vector<gameAddr> GetCharacterAddresses() override;
};
