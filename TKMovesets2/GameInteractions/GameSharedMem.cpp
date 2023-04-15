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

	// Every game has its own subtleties so we use polymorphism to manage that
	importer = Games::FactoryGetImporter(currentGameId, process, game);
	sharedMemHandler = Games::FactoryGetOnline(currentGameId, process, game);
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