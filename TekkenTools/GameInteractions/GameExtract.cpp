#include <set>
#include <chrono>
#include <thread>
#include <format>
#include <iostream>
#include <filesystem>

#include "GameExtract.hpp"
#include "GameAddressesFile.hpp"
#include "Extractor.hpp"
#include "Extractor_t7.hpp"
#include "helpers.hpp"

#include "constants.h"
#include "GameAddresses.h"

// -- Private methods -- //

void GameExtract::LoadCharacterNames()
{
	if (m_extractor != nullptr && m_extractor->CanExtract()) {
		gameAddr playerAddress = game->ReadPtr("p1_addr");
		gameAddr playerStructSize = GameAddressesFile::GetSingleValue("val_playerstruct_size");

		for (int playerId = 0; playerId < characterCount; ++playerId) {
			characterNames[playerId] = m_extractor->GetPlayerCharacterName(playerAddress + playerId * playerStructSize);
		}
	}
}

void GameExtract::OnProcessAttach()
{
	InstantiateFactory();
	LoadCharacterNames();
	m_playerAddress.clear();
}

void GameExtract::InstantiateFactory()
{
	if (m_extractor != nullptr) {
		delete m_extractor;
	}

	// Every game has its own extraction subtleties so we use polymorphism to manage that

	switch (currentGameId)
	{
	case GameId_t7:
		m_extractor = new ExtractorT7(process, game);
		break;
	case GameId_t8:
		m_extractor = nullptr;
		break;
	case GameId_ttt2:
		m_extractor = nullptr;
		break;
	}
}

void GameExtract::RunningUpdate()
{
	// Load character name for fancy live displaying : You know who you're extracting
	LoadCharacterNames();

	// Extraction queue is a FIFO (first in first out) queue
	while (m_playerAddress.size() > 0)
	{
		// Start extraction
		m_extractor->Extract(m_playerAddress[0], &progress, overwriteSameFilename);
		m_playerAddress.erase(m_playerAddress.begin());
	}
}

// -- Public methods -- //

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
	return m_playerAddress.size() > 0;
}

void GameExtract::QueueCharacterExtraction(int playerId)
{
	// It is safe to call this function even while an extraction is ongoing
	gameAddr playerAddress = game->ReadPtr("p1_addr");
	gameAddr playerStructSize = GameAddressesFile::GetSingleValue("val_playerstruct_size");

	if (playerId == -1) {
		// Queue the extraction of every character one by one
		for (playerId = 0; playerId < characterCount; ++playerId) {
			m_playerAddress.push_back(playerAddress + playerId * playerStructSize);
		}
	}
	else {
		// Queue the extraction of one character
		m_playerAddress.push_back(playerAddress + playerId * playerStructSize);
	}
}