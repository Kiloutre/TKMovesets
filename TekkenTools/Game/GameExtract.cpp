#include "GameExtract.hpp"

void GameExtract::ExtractCharacter(void* playerAddress)
{
	busy = true;
	extractionProgress = 0.0f;

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