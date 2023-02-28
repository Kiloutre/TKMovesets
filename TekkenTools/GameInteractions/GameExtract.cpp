#include "GameData.hpp"
#include "GameExtract.hpp"
#include "GameProcess.hpp"
#include "GameInteractions.hpp"

void GameExtract::ExtractCharacter(void* playerAddress)
{
	extractionProgress = 0.0f;

	process = &GameProcess::getInstance();
	//gamedata= &GameData::getInstance();
	// ...


	extractionProgress = 100.0f;
}

void GameExtract::ExtractP1()
{
	void* playerAddress = nullptr;
}

void GameExtract::ExtractP2()
{
	//ExtractCharacter();
}

void GameExtract::ExtractAll()
{
	//GameInteractions::getInstance().StartAction();
}