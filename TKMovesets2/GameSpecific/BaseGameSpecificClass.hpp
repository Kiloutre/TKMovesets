#pragma once

#include "GameData.hpp"
#include "GameProcess.hpp"

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
	// ID of the game
	uint16_t m_gameId;
	// ID of the minor version of the game
	uint16_t m_minorVersion;

public:
	BaseGameSpecificClass(GameProcess* process, GameData* game, uint16_t gameId, uint16_t minorVersion) : m_process(process), m_game(game), m_gameId(gameId), m_minorVersion(minorVersion) {}
};