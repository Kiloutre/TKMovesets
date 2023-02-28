#include <chrono>
#include <thread>

#include "GameData.hpp"
#include "GameExtract.hpp"
#include "GameProcess.hpp"
#include "GameAddresses.h"
#include "GameAddressesFile.hpp"
#include "Extractor.hpp"
#include "Extractor_t7.hpp"

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
			c_characterNames[0] = nullptr;
			process->Attach(currentGameProcess.c_str());
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

// -- Actual extraction -- //

// Todo: move in t7 file
void GameExtract::ExtractCharacter(gameAddr playerAddress)
{
	extractionProgress = 0.0f;

	Extractor* extractor = nullptr;

	switch (currentGameId)
	{
	case GameId_t7:
		extractor = new ExtractorT7(process, game);
		break;
	case GameId_t8:
		extractor = nullptr;
		break;
	case GameId_ttt2:
		extractor = nullptr;
		break;
	}

	if (extractor != nullptr) {
		extractor->Extract(playerAddress, &extractionProgress);
		delete extractor;
	}
}

// -- Interaction -- //

const char* GameExtract::GetCharacterName(int playerId)
{
	return nullptr;
}

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


void GameExtract::QueueCharacterExtraction(int playerId)
{
	gameAddr playerAddress = game->ReadPtr("p1_addr");
	gameAddr playerStructSize = GameAddressesFile::GetSingleValue("val_playerstruct_size");

	if (playerId == -1) {
		for (playerId = 0; playerId < characterCount; ++playerId) {
			OrderExtraction(playerAddress + playerId * playerStructSize);
		}
	}
	else {
		OrderExtraction(playerAddress);
	}
}