#include "GameSharedMem.hpp"

void GameSharedMem::OnProcessAttach()
{
	InstantiateFactory();
	m_plannedImportations.clear();
}

void GameSharedMem::OnProcessDetach()
{
	isMemoryLoaded = false;
	isInjecting = false;
	m_requestedInjection = false;
	displayedMovesets.clear();
}

void GameSharedMem::InstantiateFactory()
{
	// Delete old instances if needed
	if (m_importer != nullptr) {
		delete m_importer;
	}

	if (m_sharedMemHandler != nullptr) {
		delete m_sharedMemHandler;
	}

	game->gameKey = currentGame->dataString;
	game->minorGameKey = currentGame->minorDataString;
	// Every game has its own subtleties so we use polymorphism to manage that
	m_importer = Games::FactoryGetImporter(currentGame, process, game);
	m_sharedMemHandler = Games::FactoryGetOnline(currentGame, process, game);
}

void GameSharedMem::RunningUpdate()
{
	// Atempt to load the shared memory if needed
	if (!m_sharedMemHandler->IsMemoryLoaded()) {
		// Update cached variable to avoid external code having to access .sharedMemHandler 
		isMemoryLoaded = m_sharedMemHandler->LoadSharedMemory();
		if (isMemoryLoaded) {
			isInjecting = false;
			m_requestedInjection = false;
		}
		else if (isInjecting) {
			// Memory loaded, check if we were requested to inject the DLL in the past
			if (!m_requestedInjection) {
				// Order the DLL to be injected
				m_sharedMemHandler->InjectDll();
				m_requestedInjection = true;
			}
			// DLL was already ordered to be injected in the past
			else if (!m_sharedMemHandler->isInjecting) {
				// Injection was done, however the memory isn't loaded.
				DEBUG_LOG("\n --  GameSharedMem::RunningUpdate() -- ERROR: DLL Injection finished, but memory not loaded. Did the DLL fail to initialize the memory?\n");
				isInjecting = false;
				m_requestedInjection = false;
			}
		}
	}

	bool errored = false;
	while (IsBusy())
	{
		if (!errored && m_plannedImportations.size() > 0)
		{
			ImportationErrcode_ err;

			auto& [moveset, settings, playerId] = m_plannedImportations[0];
			err = m_importer->Import(moveset.filename.c_str(), 0, settings, progress);

			// Send the successfully loaded moveset to the shared memory manager
			if (err == ImportationErrcode_Successful) {
				auto& lastLoaded = m_importer->lastLoaded;
				m_sharedMemHandler->OnMovesetImport(&moveset, playerId, lastLoaded);
				// Make a copy of the displayed movesets to avoid the GUI having to iterate on a vector that might be destroyed at any time
				displayedMovesets = *m_sharedMemHandler->displayedMovesets;
			} else {
				m_errors.push_back(err);
				errored = true;
			}

			m_plannedImportations.erase(m_plannedImportations.begin());

			/*
			* TODO: think about when to free online-used movesets
			* Should check both player offsets and shared memory
			* Importer should return a list of unused movesets and then shared mem handler should be checked too before proceeding
			if (settings & ImportSettings_FreeUnusedMovesets) {
				m_importer->CleanupUnusedMovesets();
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

void GameSharedMem::InjectDll()
{
	isInjecting = true;
}