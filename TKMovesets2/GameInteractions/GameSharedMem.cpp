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
	lockedIn = false;
	displayedMovesets.clear();
}

void GameSharedMem::InstantiateFactory()
{
	// Delete old instances if needed
	m_toFree_importer = m_importer;
	m_toFree_sharedMemHandler = m_sharedMemHandler;

	game.SetCurrentGame(currentGame);
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
			versionMismatch = m_sharedMemHandler->versionMismatch;
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
	else {
		if (synchronizeLockin) {
			m_sharedMemHandler->SetLockIn(lockedIn, movesetLoaderMode);
		}
		if (m_sharedMemHandler->versionMismatch) {
			m_sharedMemHandler->VerifyDllVersionMismatch();
		}
	}


	while (IsBusy())
	{
		auto& [moveset, settings, playerId] = m_plannedImportations[0];

		// The only case where .size can be 0 is if a Submenu purposefully set it that way, to indicate that we want to clear the selection
		if (moveset.size == 0) {
			m_sharedMemHandler->ClearMovesetSelection(playerId);
			m_plannedImportations.erase(m_plannedImportations.begin());
		}
		else
		{
			ImportationErrcode_ errcode;
			errcode = m_importer->Import(moveset.filename.c_str(), 0, settings, progress);

			// Send the successfully loaded moveset to the shared memory manager
			if (errcode == ImportationErrcode_Successful) {
				auto& lastLoaded = m_importer->lastLoaded;
				m_sharedMemHandler->OnMovesetImport(&moveset, playerId, lastLoaded);
				m_plannedImportations.erase(m_plannedImportations.begin());
			} else {
				m_errors.push_back(errcode);
				m_plannedImportations.clear();
			}
		}

		// Make a copy of the displayed movesets to avoid the GUI having to iterate on a vector that might be destroyed at any time
		displayedMovesets = *m_sharedMemHandler->displayedMovesets;


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

void GameSharedMem::QueueCharacterImportation(movesetInfo* moveset, unsigned int playerId, ImportSettings settings)
{
	m_plannedImportations.push_back({
		.moveset = *moveset,
		.settings = settings,
		.playerId = playerId
	});
}

bool GameSharedMem::IsBusy() const
{
	return m_plannedImportations.size() > 0;
}

void GameSharedMem::InjectDll()
{
	isInjecting = true;
}

void GameSharedMem::StopThreadAndCleanup()
{
	// Order thread to stop
	m_threadStarted = false;
	m_t.join();

	if (m_importer != nullptr) {
		if (process.IsAttached()) {
			m_importer->CleanupUnusedMovesets();
		}
		delete m_sharedMemHandler;
		delete m_importer;
	}
}

void GameSharedMem::FreeExpiredFactoryClasses()
{
	if (m_toFree_importer) {
		delete m_toFree_importer;
		delete m_toFree_sharedMemHandler;

		if (m_toFree_importer == m_importer) {
			m_importer = nullptr;
		}
		if (m_toFree_sharedMemHandler == m_sharedMemHandler) {
			m_sharedMemHandler = nullptr;
		}

		m_toFree_importer = nullptr;
		m_toFree_sharedMemHandler = nullptr;
	}
}

void GameSharedMem::ResetTargetProcess()
{
	if (process.IsAttached())
	{
		PreProcessDetach();
		process.Detach();
		OnProcessDetach();
	}

	currentGame = nullptr;

	if (m_importer != nullptr) {
		m_toFree_importer = m_importer;
	}

	if (m_sharedMemHandler != nullptr) {
		m_toFree_sharedMemHandler = m_sharedMemHandler;
	}
}