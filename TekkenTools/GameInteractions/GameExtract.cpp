#include <chrono>
#include <thread>

#include "GameData.hpp"
#include "GameExtract.hpp"
#include "GameProcess.hpp"
#include "GameAddressesFile.hpp"
#include "Extractor.hpp"
#include "Extractor_t7.hpp"

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

void GameExtract::LoadCharacterNames()
{
	gameAddr playerAddress = game->ReadPtr("p1_addr");
	gameAddr playerStructSize = GameAddressesFile::GetSingleValue("val_playerstruct_size");

	for (int playerId = 0; playerId < characterCount; ++playerId) {
		characterNames[playerId] = m_extractor->GetPlayerCharacterName(playerAddress + playerId * playerStructSize);
	}
}

void GameExtract::Update()
{
	while (m_threadStarted)
	{
		if (process->IsAttached() && process->CheckRunning()) {
			LoadCharacterNames();
			while (m_playerAddress.size() > 0)
			{
				// Start extraction
				m_extractor->Extract(m_playerAddress[0], &progress);
				m_playerAddress.erase(m_playerAddress.begin());
			}
		}
		else if (currentGameId != -1) {
			// Process closed, try to attach again in case it is restarted
			if (process->Attach(currentGameProcess.c_str())) {
				OnProcessAttach();
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
}

void GameExtract::OnProcessAttach()
{
	InstantiateExtractor();
	LoadCharacterNames();
}

void GameExtract::InstantiateExtractor()
{
	if (m_extractor != nullptr) {
		delete m_extractor;
	}

	switch (currentGameId)
	{
	case GameId_t7:
		m_extractor = new ExtractorT7(process, game);
		break;
	case GameId_t8:
		m_extractor = nullptr;
		break;
	case GameId_ttt2:
		m_extractor = nullptr;
		break;
	}
}

// -- Interaction -- //

void GameExtract::SetTargetProcess(const char* processName, size_t gameId)
{
	if (IsBusy()) {
		return;
	}

	if (process->IsAttached()) process->Detach();

	currentGameProcess = std::string(processName);
	currentGameId = gameId;
	if (process->Attach(processName)) {
		OnProcessAttach();
	}
}

bool GameExtract::CanExtract()
{
	return m_extractor->CanExtract();
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