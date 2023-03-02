#pragma once

#include <string>

#include "GameData.hpp"
#include "GameProcess.hpp"
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
private:
	// Callback called whenever the process is re-atached
	virtual void OnProcessAttach();
	// Instantiate an extractor with polymorphism, also destroy the old one
	virtual void InstantiateFactory() = 0;

public:
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