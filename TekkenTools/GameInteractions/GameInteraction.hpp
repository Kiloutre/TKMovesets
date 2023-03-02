#pragma once

#include <string>

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "LocalStorage.hpp"
#include "ThreadedClass.hpp"

#include "GameAddresses.h"

// Contains a unique ID for every supported game. Every list should respect the index order here
enum GameId
{
	GameId_t7 = 0,
	GameId_t8 = 1,
	GameId_ttt2 = 2,
};

class GameInteraction : public ThreadedClass
{
protected:
	// Instantiate an extractor with polymorphism, also destroy the old one
	virtual void InstantiateFactory() = 0;
	// Function ran in the parallel thread, executed by Update() only if latched on and if CanStart() returned true
	virtual void RunningUpdate() = 0;
	// Callback called when the process has latched on a process for the first time
	virtual void OnProcessAttach() = 0;

	// Function ran in the parallel thread, executed regularly
	void Update();
public:
	// Everything that is related to files is done through .storage. Listing, deleting, etc.
	LocalStorage* storage = nullptr;
	// Stores the ID of the currently opened game
	size_t currentGameId = -1;
	// Used to know which process is currently being searched/opened
	std::string currentGameProcess;
	// Currently opened process for extraction
	GameProcess* process = nullptr;
	// Helper to read on address from their game_addresses.txt identifier
	GameData* game = nullptr;
	// Progress of the current task, between 0.0f and 100.0f
	float progress{ 0.0f };
	// Max character count of the game
	int characterCount = 2;

	// Returns true if the extractor will allow an extraction (false if it won't, like if characters aren't loaded)
	virtual bool CanStart() = 0;
	// Is currently busy
	virtual bool IsBusy() = 0;
	// Set the process to open
	void SetTargetProcess(const char* processName, size_t gameId);
};