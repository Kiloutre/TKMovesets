#include <set>
#include <chrono>
#include <thread>
#include <format>
#include <iostream>
#include <filesystem>

#include "GameImport.hpp"
#include "Games.hpp"
#include "Importer.hpp"
#include "helpers.hpp"

#include "constants.h"
#include "GameAddresses.h"

/// Class storing an importation queue and an Importer* instance to feed it to

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
	m_importer = Games::FactoryGetImporter(currentGameId, process, game);
}

void GameImport::PreProcessDetach()
{
	if (free_unused_movesets) {
		m_importer->CleanupUnusedMovesets();
	}
}

void GameImport::RunningUpdate()
{

	// Extraction queue is a FIFO (first in first out) queue
	while (m_plannedImportations.size() > 0)
	{
		auto &[filename, playerAddress] = m_plannedImportations[0];
		// Start Importation
		ImportationErrcode err = m_importer->Import(filename.c_str(), playerAddress, apply_instantly, progress);
		if (free_unused_movesets) {
			m_importer->CleanupUnusedMovesets();
		}
		if (err != ImportationSuccessful) {
			m_errors.push_back(err);
		}
		m_plannedImportations.erase(m_plannedImportations.begin());
	}
}
// -- Public methods -- //

ImportationErrcode GameImport::GetLastError()
{
	if (m_errors.size() > 0) {
		ImportationErrcode err = m_errors[0];
		m_errors.erase(m_errors.begin());
	}
	return ImportationSuccessful;
}

void GameImport::StopThreadAndCleanup()
{
	// Order thread to stop
	m_threadStarted = false;
	m_t.join();

	if (m_importer != nullptr) {
		if (free_unused_movesets && process->IsAttached()) {
			m_importer->CleanupUnusedMovesets();
		}
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
	gameAddr playerStructSize = game->addrFile->GetSingleValue("val_playerstruct_size");
	gameAddr playerAddress = game->ReadPtr("p1_addr") + currentPlayerId * playerStructSize;

	m_plannedImportations.push_back(std::pair<std::string, gameAddr>(filename, playerAddress));
}
