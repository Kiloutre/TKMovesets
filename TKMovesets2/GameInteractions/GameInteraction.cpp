#include "helpers.hpp"

#include "constants.h"
#include "GameInteraction.hpp"

// -- Private methods -- //


// -- Public methods -- //

void GameInteraction::SetTargetProcess(const GameInfo* gameInfo)
{
	if (IsBusy()) {
		return;
	}

	currentGame = gameInfo;
	currentGameProcess = std::string(gameInfo->processName);

	if (process->IsAttached()) {
		PreProcessDetach();
		process->Detach();
		OnProcessDetach();
	}
}

void GameInteraction::Update()
{
	// Executed in its own thread, is the one in charge of extraction, that way it won't interrupt the GUI rendering
	while (m_threadStarted)
	{
		// Ensure the process is still open and valid before possibly extracting
		if (process->IsAttached() && process->CheckRunning()) {
			process->FreeOldGameMemory();
			if (CanStart(false)) {
				actionStartDate = Helpers::getCurrentTimestamp();
				m_canStart = true;
				RunningUpdate();
			}
			else {
				m_canStart = false;
			}
		}
		else if (currentGame != nullptr) {
			// Process closed, try to attach again in case it is restarted
			if (process->Attach(currentGameProcess.c_str(), m_processExtraFlags)) {
				DEBUG_LOG("Process is now attached - OnProcessAttach()\n");
				OnProcessAttach();
			}
		}

		if (m_lastStatus != process->status) {
			if (m_lastStatus == GameProcessErrcode_PROC_ATTACHED) {
				DEBUG_LOG("Process is not attached anymore - OnProcessDetach()\n");
				OnProcessDetach();
			}
			m_lastStatus = process->status;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(GAME_INTERACTION_THREAD_SLEEP_MS));
	}
}

void GameInteraction::Init(GameAddressesFile* addrFile, LocalStorage* t_storage)
{
	// todo: Clean this up, we don't need to instantiate multiple GameData classes.
    // or maybe we do for data strings?
	process = new GameProcess;
	game = new GameData(process, addrFile);
	storage = t_storage;
}

bool GameInteraction::IsAttached() const
{
	return process != nullptr && process->IsAttached();
}

void GameInteraction::StopThreadAndCleanup()
{
	// Order thread to stop
	m_threadStarted = false;
	m_t.join();

	delete process;
	delete game;
}

uint8_t GameInteraction::GetCharacterCount() const
{
	if (currentGame != nullptr) {
		return currentGame->characterCount;
	}
	return 2;
}