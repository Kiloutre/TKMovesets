#include <set>
#include <thread>
#include <format>
#include <iostream>
#include <filesystem>

#include "GameExtract.hpp"
#include "Games.hpp"
#include "Extractor.hpp"
#include "helpers.hpp"

#include "constants.h"
#include "GameTypes.h"

/// Class storing an extraction queue and an Extractor* instance to feed it to

// -- Private methods -- //

void GameExtract::LoadCharacterNames()
{
	if (m_extractor != nullptr && m_extractor->CanExtract()) {

		uint8_t playerId = 0;
		for (gameAddr playerAddress : m_extractor->GetCharacterAddresses()) {
			characterNames[playerId++] = m_extractor->GetPlayerCharacterName(playerAddress);
		}
	}
}

void GameExtract::OnProcessAttach()
{
	InstantiateFactory();
	LoadCharacterNames();
	m_extractionQueue.clear();
}

void GameExtract::InstantiateFactory()
{
	if (m_extractor != nullptr) {
		delete m_extractor;
	}

	game->gameKey = currentGame->dataString;
	game->minorGameKey = currentGame->minorDataString;
	m_extractor = Games::FactoryGetExtractor(currentGame, process, game);
}

void GameExtract::RunningUpdate()
{
	// Load character name for fancy live displaying : You know who you're extracting
	LoadCharacterNames();

	// Extraction queue is a FIFO (first in first out) queue
	bool errored = false;
	while (m_extractionQueue.size() > 0)
	{
		if (!errored) {
			// Start extraction
			auto&[ playerAddress, settings ] = m_extractionQueue[0];
			ExtractionErrcode_ err = m_extractor->Extract(playerAddress, settings, progress);
			if (err != ExtractionErrcode_Successful) {
				m_errors.push_back(err);
				errored = true;
			}
		}
		m_extractionQueue.erase(m_extractionQueue.begin());
	}
}

// -- Public methods -- //

ExtractionErrcode_ GameExtract::GetLastError()
{
	if (m_errors.size() > 0) {
		ExtractionErrcode_ err = m_errors[0];
		m_errors.erase(m_errors.begin());
		return err;
	}
	return ExtractionErrcode_Successful;
}

void GameExtract::StopThreadAndCleanup()
{
	// Order thread to stop
	m_threadStarted = false;
	m_t.join();

	if (m_extractor != nullptr) {
		delete m_extractor;
	}

	delete process;
	delete game;
}

bool GameExtract::CanStart(bool cached)
{
	if (m_extractor == nullptr) {
		return false;
	}
	if (cached) {
		return m_canStart;
	}
	// Per-game definition
	return m_extractor->CanExtract();
}

bool GameExtract::IsBusy()
{
	// There are still playerAddresss to extract from
	return m_extractionQueue.size() > 0;
}

void GameExtract::QueueCharacterExtraction(int playerId, ExtractSettings settings)
{
	// It is safe to call this function even while an extraction is ongoing

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

uint8_t GameExtract::GetCharacterCount()
{
	if (m_extractor != nullptr) {
		return m_extractor->characterCount;
	}
	return 2;
}