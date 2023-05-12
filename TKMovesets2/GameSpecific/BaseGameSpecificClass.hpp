#pragma once

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "GameInfo.hpp"

#include "constants.h"
#include "GameTypes.h"

// Base class for interacting with a game
class DLLCONTENT BaseGameSpecificClass
{
protected:
	// Stores the process to read on
	GameProcess* m_process;
	// Stores a helper class to read the game's memory from strings in game_addresses.txt
	GameData* m_game;
	// Structure containing ID of the game, minor version, an dmore
	const GameInfo m_gameInfo;

public:
	BaseGameSpecificClass(GameProcess* process, GameData* game, const GameInfo* gameInfo) : m_process(process), m_game(game), m_gameInfo(*gameInfo) {
		m_game->minorGameKey = m_gameInfo.minorDataString;
	}
};