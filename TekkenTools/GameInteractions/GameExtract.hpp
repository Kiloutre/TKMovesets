#pragma once

#include <vector>

#include "GameProcess.hpp"

class GameExtract
{
private:
	GameExtract() = default;
	~GameExtract() = default;
	GameExtract& operator = (const GameExtract&) = delete;
	GameExtract(const GameExtract&) = delete;

	// Is a thread running
	bool m_threadStarted = false;
	// Player address to extract
	std::vector<void*> m_playerAddress;

	// Order an extraction to be pushed to the queue
	void OrderExtraction(void* playerAddress);
	// Regularly called in an isolated thread to execute queued extractions
	void Update();
	// Extract a character from its address
	void ExtractCharacter(void* playerAddress);
public:
	static GameExtract& getInstance() {
		// Todo: mutex here or something?
		static GameExtract s_instance;
		return s_instance;
	}

	// Stores the ID of the currently opened game
	size_t currentGameId = 0;
	// Currently opened process for extraction
	GameProcess* process;
	// Progress of the current extraction, between 0.0f and 100.0f
	float extractionProgress{ 0.0f };

	// Is curently busy with an extraction
	bool IsBusy();
	// Set the process to open
	void SetTargetProcess(const char* processName, size_t gameId);

	// Extracts only the first player's selected character
	void ExtractP1();
	// Extracts only the second player's selected character
	void ExtractP2();
	// Extracts every player character 
	void ExtractAll();
};