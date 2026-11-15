#pragma once
#include "Extractor.hpp"
#include "GameStaticData.hpp"

// Retail records and PANM resources; no dependency on CNT record structures.
class ExtractorT8 : public Extractor {
    std::string m_lastError;
    const GameStaticData m_names = GameStaticData::LoadOptional("t8-retail");
public:
    using Extractor::Extractor;
    const std::string& LastError() const { return m_lastError; }
    ExtractionErrcode_ Extract(gameAddr player, ExtractSettings settings, uint8_t& progress) override;
    bool CanExtract() override;
    std::string GetPlayerCharacterName(gameAddr player) override;
    uint32_t GetCharacterID(gameAddr player) override;
    gameAddr GetCharacterAddress(uint8_t player) override;
    std::vector<gameAddr> GetCharacterAddresses() override;
};
