#pragma once

#include <set>
#include <sys/stat.h>

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "Extractor.hpp"

#include "GameAddresses.h"

// Contains a unique ID for every supported game. Every list should respect the index order here
enum GameId
{
	GameId_t7 = 0,
	GameId_t8 = 1,
	GameId_ttt2 = 2,
};

// Moveset info, most of which is read straight from the file. Use for display.
struct movesetInfo
{
	std::string filename;
	std::string name;
	std::string origin;
	std::string target_character;
	std::string date;
	uint64_t size;
	time_t modificationDate;
};

class GameExtract
{
private:
	GameExtract() = default;
	~GameExtract() = default;
	GameExtract& operator = (const GameExtract&) = delete;
	GameExtract(const GameExtract&) = delete;

	// Extractor glass, never stores an Extractor*, used for polymorphism
	Extractor* m_extractor = nullptr;
	// Is a thread running
	bool m_threadStarted = false;
	// Player address to extract
	std::vector<gameAddr> m_playerAddress;
	// A set containing the list of moveset files in the configured extraction dir. Used to determine when to fetch new movesets info
	std::set<std::string> m_extractedMovesetFilenames;

	// Callback called whenever the process is re-atached
	void OnProcessAttach();
	// Instantiate an extractor with polymorphism, also destroy the old one
	void InstantiateExtractor();
	// Order an extraction to be pushed to the queue
	void OrderExtraction(gameAddr playerAddress);
	// Latch on to process 
	void Update();
	// Reads the movesets for the players characters' names. Accessible under .characterNames
	void LoadCharacterNames();
public:
	static GameExtract& getInstance() {
		// Todo: mutex here or something?
		static GameExtract s_instance;
		return s_instance;
	}

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
	// Contains the list of movesets found in the extraction directory
	std::vector<movesetInfo*> extractedMovesets;
	// Determine whether to ovewrite existing movesets when extracting or to append a number to the name
	bool overwriteSameFilename = false;

	// Returns true if the extractor will allow an extraction (false if it won't, like if characters aren't loaded)
	bool CanExtract();
	// Is currently busy with an extraction
	bool IsBusy();
	// Set the process to open
	void SetTargetProcess(const char* processName, size_t gameId);
	// Starts the thread that will later be used for extracton
	void StartThread();
	// Queue a character extraction. -1 of all characters
	void QueueCharacterExtraction(int playerId);
	// Reads movesets from their configured extraction dir. Accessible under .extractedMovesets
	void ReloadMovesetList();
};