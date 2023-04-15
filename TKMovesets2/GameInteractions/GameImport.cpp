#include <set>
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

void GameImport::OnProcessDetach()
{
	// There is no way to know if the moveset is still valid after detaching: reset it
	lastLoadedMoveset = 0;
}

void GameImport::InstantiateFactory()
{
	if (importer != nullptr) {
		delete importer;
	}

	// Every game has its own importation subtleties so we use polymorphism to manage that
	importer = Games::FactoryGetImporter(currentGameId, process, game);
}

void GameImport::PreProcessDetach()
{
	importer->CleanupUnusedMovesets();
}

void GameImport::RunningUpdate()
{
	bool errored = false;
	while (IsBusy())
	{
		if (!errored) {
			ImportationErrcode_ err;

			// Two different ways to import movesets, from filename or from actual moveset data
			if (m_plannedImportations.size() > 0) {
				auto& [moveset, movesetSize, filename, playerAddress, settings] = m_plannedImportations[0];
				if (moveset == nullptr) {
					err = importer->Import(filename.c_str(), playerAddress, settings, progress);
				}
				else {
					err = importer->Import(moveset, movesetSize, playerAddress, settings, progress);
				}
				m_plannedImportations.erase(m_plannedImportations.begin());

				if (settings & ImportSettings_FreeUnusedMovesets) {
					importer->CleanupUnusedMovesets();
				}
			}

			if (err != ImportationErrcode_Successful) {
				m_errors.push_back(err);
				errored = true;
			}
			lastLoadedMoveset = importer->lastLoadedMoveset;
		}
	}
}

// -- Public methods -- //

ImportationErrcode_ GameImport::GetLastError()
{
	if (m_errors.size() > 0) {
		ImportationErrcode_ err = m_errors[0];
		m_errors.erase(m_errors.begin());
	}
	return ImportationErrcode_Successful;
}

void GameImport::StopThreadAndCleanup()
{
	// Order thread to stop
	m_threadStarted = false;
	m_t.join();

	if (importer != nullptr) {
		if (process->IsAttached()) {
			importer->CleanupUnusedMovesets();
		}
		delete importer;
	}

	delete process;
	delete game;
}

bool GameImport::CanStart()
{
	if (importer == nullptr) {
		return false;
	}
	// Per-game definition
	return importer->CanImport();
}

bool GameImport::IsBusy()
{
	// There are still movesets to import
	return m_plannedImportations.size() > 0;
}

void GameImport::QueueCharacterImportation(const Byte* moveset, uint64_t movesetSize, ImportSettings settings)
{
	// It is safe to call this function even while an extraction is ongoing
	gameAddr playerAddress = importer->GetCharacterAddress(currentPlayerId);
	m_plannedImportations.push_back({
		.moveset = moveset,
		.movesetSize = movesetSize,
		.filename = "",
		.playerAddress = playerAddress,
		.settings = settings
	});
}

void GameImport::QueueCharacterImportation(std::string filename, ImportSettings settings)
{
	// It is safe to call this function even while an extraction is ongoing
	gameAddr playerAddress = importer->GetCharacterAddress(currentPlayerId);
	m_plannedImportations.push_back({
		.moveset = nullptr,
		.movesetSize = 0,
		.filename = filename,
		.playerAddress = playerAddress,
		.settings = settings
	});
}

uint8_t GameImport::GetCharacterCount()
{
	if (importer != nullptr) {
		return importer->characterCount;
	}
	// Return 1 by default on purpose, easier to debug stuff that way
	return 1;
}