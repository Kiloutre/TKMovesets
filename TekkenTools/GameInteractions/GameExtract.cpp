#include <chrono>
#include <thread>

#include "GameData.hpp"
#include "GameExtract.hpp"
#include "GameProcess.hpp"
#include "GameAddresses.h"

// -- Thread stuff -- //

void GameExtract::StartThread()
{
	if (!m_threadStarted)
	{
		m_threadStarted = true;
		std::thread t(&GameExtract::Update, this);
		t.detach();
	}
}

void GameExtract::OrderExtraction(gameAddr playerAddress)
{
	m_playerAddress.push_back(playerAddress);
}

void GameExtract::Update()
{
	while (m_threadStarted)
	{
		if (process->IsAttached() && process->CheckRunning()) {
			while (m_playerAddress.size() > 0)
			{
				ExtractCharacter(m_playerAddress.back());
				m_playerAddress.pop_back();
			}
		}
		else if (currentGameId != -1) {
			process->Attach(currentGameProcess.c_str());
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

// -- Actual extraction -- //

void GameExtract::ExtractCharacter(gameAddr playerAddress)
{
	extractionProgress = 0.0f;

	// ...


	extractionProgress = 100.0f;
}

// -- Interaction -- //

void GameExtract::SetTargetProcess(const char* processName, size_t gameId)
{
	if (IsBusy()) {
		return;
	}

	currentGameProcess = std::string(processName);
	currentGameId = gameId;
	process->Attach(processName);
}

bool GameExtract::IsBusy()
{
	return m_playerAddress.size() > 0;
}

void GameExtract::ExtractP1()
{
	gameAddr playerAddress = game->ReadPtr("p1_addr");
	OrderExtraction(playerAddress);
}

void GameExtract::ExtractP2()
{
	gameAddr playerAddress = (gameAddr)0;
	OrderExtraction(playerAddress);
}

void GameExtract::ExtractAll()
{
	ExtractP1();
	ExtractP2();
}