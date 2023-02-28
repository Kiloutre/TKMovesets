#include "GameExtract.hpp"
#include "GameProcess.hpp"

void GameExtract::ExtractCharacter(void* playerAddress)
{
	busy = true;
	extractionProgress = 0.0f;
	process = &GameProcess::getInstance();

	// ...


	extractionProgress = 100.0f;
}

void GameExtract::ExtractP1()
{
	//ExtractCharacter();
}

void GameExtract::ExtractP2()
{
	//ExtractCharacter();
}

void GameExtract::ExtractAll()
{
	ExtractP1();
	ExtractP2();
}