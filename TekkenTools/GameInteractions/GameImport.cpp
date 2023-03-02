#include <set>
#include <chrono>
#include <thread>
#include <format>
#include <iostream>
#include <filesystem>

#include "GameImport.hpp"
#include "GameAddressesFile.hpp"
#include "Importer.hpp"
#include "Importer_t7.hpp"
#include "helpers.hpp"

#include "constants.h"
#include "GameAddresses.h"

// -- Private methods -- //

void GameImport::OnProcessAttach()
{
	InstantiateFactory();
	m_plannedImportations.clear();
}


void GameImport::InstantiateFactory()
{
	if (m_importer != nullptr) {
		delete m_importer;
	}

	// Every game has its own extraction subtleties so we use polymorphism to manage that

	switch (currentGameId)
	{
	case GameId_t7:
		m_importer = new ImporterT7(process, game);
		break;
	case GameId_t8:
		m_importer = nullptr;
		break;
	case GameId_ttt2:
		m_importer = nullptr;
		break;
	}
}


void GameImport::RunningUpdate()
{

	// Extraction queue is a FIFO (first in first out) queue
	while (m_plannedImportations.size() > 0)
	{
		auto &[filename, playerAddress] = m_plannedImportations[0];
		// Start Importation
		m_importer->Import(filename.c_str(), playerAddress, &progress);
		m_plannedImportations.erase(m_plannedImportations.begin());
	}
}

// -- Public methods -- //

void GameImport::StopThreadAndCleanup()
{
	// Order thread to stop
	m_threadStarted = false;
	m_t.join();

	if (m_importer != nullptr) {
		delete m_importer;
	}
}

bool GameImport::CanStart()
{
	if (m_importer == nullptr) {
		return false;
	}
	// Per-game definition
	return m_importer->CanImport();
}

bool GameImport::IsBusy()
{
	// There are still playerAddresss to extract from
	return m_plannedImportations.size() > 0;
}

void GameImport::QueueCharacterImportation(std::string filename)
{
	// It is safe to call this function even while an extraction is ongoing
	gameAddr playerStructSize = GameAddressesFile::GetSingleValue("val_playerstruct_size");
	gameAddr playerAddress = game->ReadPtr("p1_addr") + currentPlayerId * playerStructSize;

	m_plannedImportations.push_back(std::pair<std::string, gameAddr>(filename, playerAddress));
}
