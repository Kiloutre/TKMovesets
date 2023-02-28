#pragma once

#include "GameInteractions.hpp"

class GameInteractions
{
private:
	GameInteractions() = default;
	~GameInteractions() = default;
	GameInteractions& operator = (const GameInteractions&) = delete;
	GameInteractions(const GameInteractions&) = delete;

	//
	void* playerAddress;
	// Callback that will be called during the next Update()
	void (GameExtract::* actionToCall)(void*) = nullptr;
public:
	static GameInteractions& getInstance() {
		// Todo: mutex here or something?
		static GameInteractions s_instance;
		return s_instance;
	}

	// true = Currently busy with an action (extraction, importation).
	bool busy{ false };
	// Used to know whether to create the thread or not when an action is given
	bool threadStarted{ false };

	// Orders an action that will be executed in its own thread
	void StartAction();

	// Regularly called in its own thread, executes queued actions
	void Update();
};