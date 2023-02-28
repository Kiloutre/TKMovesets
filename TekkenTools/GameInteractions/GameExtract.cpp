#include <chrono>
#include <thread>

#include "GameData.hpp"
#include "GameExtract.hpp"
#include "GameProcess.hpp"

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

void GameExtract::OrderExtraction(void *playerAddress)
{
	m_playerAddress.push_back(playerAddress);
}

void GameExtract::Update()
{
	while (m_threadStarted)
	{
		while (m_playerAddress.size() > 0)
		{
			ExtractCharacter(m_playerAddress.back());
			m_playerAddress.pop_back();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

// -- Actual extraction -- //

void GameExtract::ExtractCharacter(void* playerAddress)
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
	void* playerAddress = nullptr;
	OrderExtraction(playerAddress);
}

void GameExtract::ExtractP2()
{
	void* playerAddress = nullptr;
	OrderExtraction(playerAddress);
}

void GameExtract::ExtractAll()
{
	ExtractP1();
	ExtractP2();
}