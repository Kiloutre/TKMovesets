#pragma once

#include "GameInteractions.hpp"

class GameInteractions
{
private:
	GameInteractions() = default;
	~GameInteractions() = default;
	GameInteractions& operator = (const GameInteractions&) = delete;
	GameInteractions(const GameInteractions&) = delete;

	// Holds a reference to the progress variable to pass on to the action
	float& progress;
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
	//void GameInteractions::StartAction(void* callback, float& progress);

	// Regularly called in its own thread, executes queued actions
	void Update();
};