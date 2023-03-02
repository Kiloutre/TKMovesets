#pragma once

#include <set>
#include <sys/stat.h>

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "Extractor.hpp"
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

class GameExtract : public ThreadedClass
{
private:
	// Extractor glass, never stores an Extractor*, used for polymorphism
	Extractor* m_extractor = nullptr;
	// Player address to extract
	std::vector<gameAddr> m_playerAddress;
	// .extractedMovesets garbage, because it can be accessed in another thread while we remove items for it.
	std::vector<movesetInfo*> m_garbage;

	// Callback called whenever the process is re-atached
	void OnProcessAttach();
	// Instantiate an extractor with polymorphism, also destroy the old one
	void InstantiateExtractor();
	// Reads the movesets for the players characters' names. Accessible under .characterNames
	void LoadCharacterNames();
	// Function ran in the parallel thread, used to latch on to process
	void Update() override;
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
	// Progress of the current extraction, between 0.0f and 100.0f
	float progress{ 0.0f };
	// Contains the character names
	std::string characterNames[4];
	// Max character count of the game
	int characterCount = 2;
	// Determine whether to ovewrite existing movesets when extracting or to append a number to the name
	bool overwriteSameFilename = false;

	void StartThread() override;
	// Stops the thread started above
	void StopThreadAndCleanup() override;
	// Returns true if the extractor will allow an extraction (false if it won't, like if characters aren't loaded)
	bool CanExtract();
	// Is currently busy with an extraction
	bool IsBusy();
	// Set the process to open
	void SetTargetProcess(const char* processName, size_t gameId);
	// Starts the thread that will later be used for extracton
	// Queue a character extraction. -1 of all characters
	void QueueCharacterExtraction(int playerId);
};