#include "GameSharedMem.hpp"

void GameSharedMem::OnProcessAttach()
{
	InstantiateFactory();
	m_plannedImportations.clear();
}

void GameSharedMem::InstantiateFactory()
{
	// Delete old instances if needed
	if (importer != nullptr) {
		delete importer;
	}

	if (sharedMemHandler != nullptr) {
		delete sharedMemHandler;
	}

	game->gameKey = currentGame->dataString;
	game->minorGameKey = currentGame->minorDataString;
	// Every game has its own subtleties so we use polymorphism to manage that
	importer = Games::FactoryGetImporter(currentGame, process, game);
	sharedMemHandler = Games::FactoryGetOnline(currentGame, process, game);
}

void GameSharedMem::RunningUpdate()
{
	// Atempt to load the shared memory if needed
	if (!sharedMemHandler->IsMemoryLoaded()) {
		sharedMemHandler->LoadSharedMemory();
	}

	bool errored = false;
	while (IsBusy())
	{
		if (!errored && m_plannedImportations.size() > 0)
		{
			ImportationErrcode_ err;

			auto& [moveset, settings, playerId] = m_plannedImportations[0];
			err = importer->Import(moveset.filename.c_str(), 0, settings, progress);

			// Send the successfully loaded moveset to the shared memory manager
			if (err == ImportationErrcode_Successful) {
				auto& lastLoaded = importer->lastLoaded;
				sharedMemHandler->OnMovesetImport(&moveset, playerId, lastLoaded);
				lastLoadedMoveset = importer->lastLoaded.address;
			} else {
				m_errors.push_back(err);
				errored = true;
			}

			m_plannedImportations.erase(m_plannedImportations.begin());

			/*
			* TODO: think about when to free online-used movesets
			if (settings & ImportSettings_FreeUnusedMovesets) {
				importer->CleanupUnusedMovesets();
			}
			*/
		}
	}
}

void GameSharedMem::QueueCharacterImportation(movesetInfo* moveset, unsigned int playerId, ImportSettings settings)
{
	m_plannedImportations.push_back({
		.moveset = *moveset,
		.settings = settings,
		.playerId = playerId
	});
}

bool GameSharedMem::IsBusy()
{
	return m_plannedImportations.size() > 0;
}
