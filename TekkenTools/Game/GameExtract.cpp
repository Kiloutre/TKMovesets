#include "GameExtract.hpp"

void GameExtract::ExtractP1ayer(void* playerAddress)
{
	busy = true;
	extractionProgress = 0.0f;

	// ...


	extractionProgress = 100.0f;
}

void GameExtract::ExtractP1()
{
	//ExtractP1ayer(0);
}

void GameExtract::ExtractP2()
{
	//ExtractP1ayer(0);
}

void GameExtract::ExtractAll()
{
	ExtractP1();
	ExtractP2();
}