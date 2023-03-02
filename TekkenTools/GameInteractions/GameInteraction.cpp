#include "helpers.hpp"

#include "constants.h"
#include "GameInteraction.hpp"

// -- Private methods -- //


// -- Public methods -- //

void GameInteraction::SetTargetProcess(const char* processName, uint8_t gameId)
{
	if (IsBusy()) {
		return;
	}

	if (process->IsAttached()) {
		process->Detach();
	}

	currentGameProcess = std::string(processName);
	currentGameId = gameId;
	if (process->Attach(processName)) {

		if (gameId == GameId_ttt2) {
			characterCount = 4;
		}
		else {
			characterCount = 2;
		}

		OnProcessAttach();
	}
}

void GameInteraction::Update()
{
	// Executed in its own thread, is the one in charge of extraction, that way it won't interrupt the GUI rendering
	while (m_threadStarted)
	{
		// Ensure the process is still open and valid before possibly extracting
		if (process->IsAttached() && process->CheckRunning()) {
			if (CanStart()) {
				RunningUpdate();
			}
		}
		else if (currentGameId != -1) {
			// Process closed, try to attach again in case it is restarted
			if (process->Attach(currentGameProcess.c_str())) {
				OnProcessAttach();
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(GAME_INTERACTION_THREAD_SLEEP_MS));
	}
}