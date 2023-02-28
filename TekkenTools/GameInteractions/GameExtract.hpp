#pragma once

#include "GameProcess.hpp"

class GameExtract
{
private:
	GameExtract() = default;
	~GameExtract() = default;
	GameExtract& operator = (const GameExtract&) = delete;
	GameExtract(const GameExtract&) = delete;

	// Extract a character from its address
	void ExtractCharacter(void *playerAddress);
public:
	static GameExtract& getInstance() {
		// Todo: mutex here or something?
		static GameExtract s_instance;
		return s_instance;
	}

	// true = Currently busy with an extraction.
	bool busy{ false };
	// Progress of the current extraction, between 0.0f and 100.0f
	float extractionProgress{ 0.0f };
	GameProcess* process;

	// Extracts only the first player's selected character
	void ExtractP1();
	// Extracts only the second player's selected character
	void ExtractP2();
	// Extracts every player character 
	void ExtractAll();
};