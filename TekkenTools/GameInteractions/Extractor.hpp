#pragma once

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "GameAddresses.h"
#include "GameAddressesFile.hpp"

class Extractor
{
public:
	Extractor(GameProcess* process, GameData* game) : m_process(process), m_game(game) {}
	// Pure virtual base method meant to do the heavy lifting
	virtual void Extract(gameAddr playerAddress, float* progress) = 0;

protected:
	GameProcess* m_process;
	GameData* m_game;
};