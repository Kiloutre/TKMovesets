#include "helpers.hpp"

#include "constants.h"
#include "GameInteraction.hpp"

// -- Private methods -- //

void GameInteraction::OnProcessAttach()
{
	InstantiateFactory();
}

// -- Public methods -- //

void GameInteraction::SetTargetProcess(const char* processName, size_t gameId)
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
		OnProcessAttach();
	}
}

