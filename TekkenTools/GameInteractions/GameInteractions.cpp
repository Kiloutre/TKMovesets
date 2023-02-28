#include <stdarg>
#include <thread>

#include "GameData.hpp"
#include "GameInteractions.hpp"

void GameInteractions::StartAction()
{
	busy = true;
	GameData::getInstance().CacheAddresses();

	if (!threadStarted)
	{
		actionToCall = t_callback;
		threadStarted = true;
		std::thread t(&GameInteractions::Update(), this);
		t.detach();
	}
}


void GameInteractions::Update()
{
	while (true)
	{

		if (actionToCall != nullptr)
		{
			actionToCall();
			actionToCall = nullptr;
			*progress = 100.0f;
			busy = false;

		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}