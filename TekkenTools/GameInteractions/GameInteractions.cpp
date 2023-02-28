#include <stdarg>
#include <thread>

#include "GameData.hpp"
#include "GameInteractions.hpp"

void GameInteractions::StartAction(void (*callback)(va_list), float& t_progress)
{
	busy = true;
	progress = t_progress;
	*progress = 0;
	GameData::getInstance().CacheAddresses();

	if (!threadStarted)
	{
		threadStarted = true;
		std::thread t(&GameInteractions::Update, this);
		t.detach();
	}
}


void GameInteractions::Update()
{
	if (false) // if (actionOrdered)
	{
		//executeAction();
		//actionOrdered = nullptr;
		*progress = 100.0f;
		busy = false;
	}
}