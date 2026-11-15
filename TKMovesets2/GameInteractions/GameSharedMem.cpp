#include "GameSharedMem.hpp"

void GameSharedMem::OnProcessAttach()
{
	auto state = LockState();
	InstantiateFactory();
	m_plannedImportations.clear();
}

void GameSharedMem::OnProcessDetach()
{
	auto state = LockState();
	isMemoryLoaded = false;
	isInjecting = false;
	m_requestedInjection = false;
	lockedIn = false;
	versionMismatch = false;
	displayedMovesets.clear();
	if (m_sharedMemHandler) m_sharedMemHandler->FreeSharedMemory();
}

void GameSharedMem::InstantiateFactory()
{
	auto state = LockState();
	// Delete old instances if needed
	m_importer.reset();
	delete m_sharedMemHandler;
	m_sharedMemHandler = nullptr;

	game.SetCurrentGame(currentGame);
	// Game might not have an importer, but it does not matter.
	m_importer.reset(currentGame->importer == nullptr ? nullptr : Games::FactoryGetImporter(currentGame, process, game));
	m_sharedMemHandler = Games::FactoryGetOnline(currentGame, process, game);
}

void GameSharedMem::RunningUpdate()
{
	auto state = LockState();
	if (!m_sharedMemHandler) return;
	// Atempt to load the shared memory if needed
	if (!m_sharedMemHandler->IsMemoryLoaded()) {
		// Update cached variable to avoid external code having to access .sharedMemHandler 
		isMemoryLoaded = m_sharedMemHandler->LoadSharedMemory();
		if (isMemoryLoaded) {
			isInjecting = false;
			m_requestedInjection = false;
			versionMismatch = m_sharedMemHandler->VerifyDllVersionMismatch();
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
		// Checked on every tick: the loader may still have been writing its version when the memory got loaded
		versionMismatch = m_sharedMemHandler->VerifyDllVersionMismatch();
	}


	while (IsBusy())
	{
		auto [moveset, settings, playerId] = m_plannedImportations[0];

		// The only case where .size can be 0 is if a Submenu purposefully set it that way, to indicate that we want to clear the selection
		if (moveset.size == 0) {
			m_sharedMemHandler->ClearMovesetSelection(playerId);
			m_plannedImportations.erase(m_plannedImportations.begin());
		}
		else
		{
			ImportationErrcode_ errcode;
            uint8_t operationProgress = 0;
            progress = 0;
            errcode = RunOutsideState(state, [&] {
                return m_importer ? m_importer->Import(moveset.filename.c_str(), 0, settings, operationProgress) : ImportationErrcode_MovesetDataError;
            });
            progress = operationProgress;

			// Send the successfully loaded moveset to the shared memory manager
			if (errcode == ImportationErrcode_Successful) {
				auto& lastLoaded = m_importer->lastLoaded;
				m_sharedMemHandler->OnMovesetImport(&moveset, playerId, lastLoaded);
				m_plannedImportations.erase(m_plannedImportations.begin());
			} else {
				m_errors.emplace_back(errcode,m_importer ? m_importer->LastError() : std::string{});
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
	auto state = LockState();
	if (!moveset || playerId >= GetCharacterCount()) return;
	m_plannedImportations.push_back({
		.moveset = *moveset,
		.settings = settings,
		.playerId = playerId
	});
}

bool GameSharedMem::IsBusy() const
{
	auto state = LockState();
	return m_executingOperation || m_plannedImportations.size() > 0;
}

void GameSharedMem::InjectDll()
{
	auto state = LockState();
	isInjecting = true;
}

void GameSharedMem::StopThreadAndCleanup()
{
	// Order thread to stop
	ThreadedClass::StopThreadAndCleanup();
	auto state = LockState();

	if (m_sharedMemHandler != nullptr) {
		if (process.IsAttached() && m_importer != nullptr) {
			m_importer->CleanupUnusedMovesets();
		}
		delete m_sharedMemHandler;
		m_importer.reset();
		m_sharedMemHandler = nullptr;
	}
	m_plannedImportations.clear();
}

void GameSharedMem::FreeExpiredFactoryClasses()
{
    // Factory lifetime is synchronized with the worker and rendering frame.
}

void GameSharedMem::ResetTargetProcess()
{
    GameInteraction::ResetTargetProcess();
}
