#include <set>
#include <chrono>
#include <thread>
#include <format>
#include <iostream>
#include <filesystem>

#include "GameExtract.hpp"
#include "Games.hpp"
#include "Extractor.hpp"
#include "helpers.hpp"

#include "constants.h"
#include "GameAddresses.h"

/// Class storing an extraction queue and an Extractor* instance to feed it to

// -- Private methods -- //

void GameExtract::LoadCharacterNames()
{
	if (m_extractor != nullptr && m_extractor->CanExtract()) {
		gameAddr playerAddress = game->ReadPtr("p1_addr");
		gameAddr playerStructSize = game->addrFile->GetSingleValue("val_playerstruct_size");

		for (int playerId = 0; playerId < characterCount; ++playerId) {
			characterNames[playerId] = m_extractor->GetPlayerCharacterName(playerAddress + playerId * playerStructSize);
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
	m_extractor = Games::FactoryGetExtractor(currentGameId, process, game);
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
			ExtractionErrcode err = m_extractor->Extract(playerAddress, settings, currentGameId, progress);
			if (err != ExtractionSuccessful) {
				m_errors.push_back(err);
				errored = true;
			}
		}
		m_extractionQueue.erase(m_extractionQueue.begin());
	}
}

// -- Public methods -- //

ExtractionErrcode GameExtract::GetLastError()
{
	if (m_errors.size() > 0) {
		ExtractionErrcode err = m_errors[0];
		m_errors.erase(m_errors.begin());
		return err;
	}
	return ExtractionSuccessful;
}

void GameExtract::StopThreadAndCleanup()
{
	// Order thread to stop
	m_threadStarted = false;
	m_t.join();

	if (m_extractor != nullptr) {
		delete m_extractor;
	}
}

bool GameExtract::CanStart()
{
	if (m_extractor == nullptr) {
		return false;
	}
	// Per-game definition
	return m_extractor->CanExtract();
}

bool GameExtract::IsBusy()
{
	// There are still playerAddresss to extract from
	return m_extractionQueue.size() > 0;
}

void GameExtract::QueueCharacterExtraction(int playerId, ExtractionOptions::Settings settings)
{
	// It is safe to call this function even while an extraction is ongoing
	gameAddr playerAddress = game->ReadPtr("p1_addr");
	gameAddr playerStructSize = game->addrFile->GetSingleValue("val_playerstruct_size");

	if (playerId == -1) {
		// Queue the extraction of every character one by one
		for (playerId = 0; playerId < characterCount; ++playerId) {
			gameAddr pAddr = playerAddress + playerId * playerStructSize;
			m_extractionQueue.push_back(std::pair<gameAddr, ExtractionOptions::Settings>(pAddr, settings));
		}
	}
	else {
		// Queue the extraction of one character
		gameAddr pAddr = playerAddress + playerId * playerStructSize;
		m_extractionQueue.push_back(std::pair<gameAddr, ExtractionOptions::Settings>(pAddr, settings));
	}
}
