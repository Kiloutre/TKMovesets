#include "GameExtract.hpp"
#include "Games.hpp"
#include "Extractor.hpp"
#include "Extractor_t8/Extractor_t8.hpp"
#include "helpers.hpp"

#include "constants.h"
#include "GameTypes.h"

/// Class storing an extraction queue and an Extractor* instance to feed it to

// -- Private methods -- //

void GameExtract::LoadCharacterNames()
{
	auto state = LockState();
	uint8_t playerId = 0;
	for (auto& name : characterNames) name.clear();
	for (; playerId < GetCharacterCount() && playerId < std::size(characterNames); ++playerId) {
		characterNames[playerId] = m_extractor->GetPlayerCharacterName(m_extractor->GetCharacterAddress(playerId));
	}
}

void GameExtract::OnProcessAttach()
{
	auto state = LockState();
	InstantiateFactory();
	LoadCharacterNames();
	m_extractionQueue.clear();
}

void GameExtract::InstantiateFactory()
{
	auto state = LockState();
	m_extractor.reset();

	game.SetCurrentGame(currentGame);
	m_extractor.reset(Games::FactoryGetExtractor(currentGame, process, game));
}

void GameExtract::RunningUpdate()
{
	auto state = LockState();
	// Load character name for fancy live displaying : You know who you're extracting
	LoadCharacterNames();

	// Extraction queue is a FIFO (first in first out) queue
	bool errored = false;
	while (m_extractionQueue.size() > 0)
	{
		if (!errored) {
			// Start extraction
            auto [playerAddress, settings] = m_extractionQueue[0];
            uint8_t operationProgress = 0;
            progress = 0;
            ExtractionErrcode_ err = RunOutsideState(state, [&] {
                return m_extractor->Extract(playerAddress, settings, operationProgress);
            });
            progress = operationProgress;
			if (err != ExtractionErrcode_Successful) {
				// Retail T8 keeps a detailed message (e.g. which animations are missing and what to do about it)
				std::string detail;
				if (const auto* retail = dynamic_cast<const ExtractorT8*>(m_extractor.get())) detail = retail->LastError();
				m_errors.emplace_back(err, std::move(detail));
				errored = true;
			}
		}
		m_extractionQueue.erase(m_extractionQueue.begin());
	}
}

// -- Public methods -- //

ExtractionErrcode_ GameExtract::GetLastError(std::string* detail)
{
	auto state = LockState();
	if (m_errors.size() > 0) {
		ExtractionErrcode_ err = m_errors[0].first;
		if (detail) *detail = m_errors[0].second;
		m_errors.erase(m_errors.begin());
		return err;
	}
	return ExtractionErrcode_Successful;
}

void GameExtract::StopThreadAndCleanup()
{
	// Order thread to stop
	ThreadedClass::StopThreadAndCleanup();
	auto state = LockState();

	if (m_extractor != nullptr) {
		m_extractor.reset();
	}
	m_extractionQueue.clear();
}

bool GameExtract::CanStart(bool cached) const
{
	auto state = LockState();
	if (m_extractor == nullptr) {
		return false;
	}
	if (cached) {
		return m_canStart;
	}
	// Per-game definition
	return m_extractor->CanExtract();
}

bool GameExtract::IsBusy() const
{
	auto state = LockState();
	// There are still playerAddresss to extract from
	return m_executingOperation || m_extractionQueue.size() > 0;
}

void GameExtract::QueueCharacterExtraction(int playerId, ExtractSettings settings)
{
	auto state = LockState();
	if (m_executingOperation || !m_extractor || playerId < -1 || playerId >= GetCharacterCount()) return;
	// The state lock serializes queue changes with extraction.

	if (playerId == -1) {
		// Queue the extraction of every character one by one
		for (gameAddr playerAddress : m_extractor->GetCharacterAddresses()) {
			m_extractionQueue.push_back(std::pair<gameAddr, ExtractSettings>(playerAddress, settings));
		}
	}
	else {
		// Queue the extraction of one character
		gameAddr playerAddress = m_extractor->GetCharacterAddress(playerId);
		m_extractionQueue.push_back(std::pair<gameAddr, ExtractSettings>(playerAddress, settings));
	}
}

void GameExtract::FreeExpiredFactoryClasses()
{
    // Factory lifetime is synchronized with the worker and rendering frame.
}
