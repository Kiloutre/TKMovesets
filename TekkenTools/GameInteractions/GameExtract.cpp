#include <set>
#include <chrono>
#include <thread>
#include <iostream>
#include <filesystem>

#include "GameData.hpp"
#include "GameExtract.hpp"
#include "GameProcess.hpp"
#include "GameAddressesFile.hpp"
#include "Extractor.hpp"
#include "Extractor_t7.hpp"
#include "helpers.hpp"

#include "constants.h"
#include "GameAddresses.h"

// Reads a movest in order to fetch its header informations such as character name, version, etc. Can return null if errors are encountered.
static movesetInfo* fetchMovesetInformations(std::string filename)
{
	std::ifstream file;
	file.open(filename, std::ios::binary);

	if (file.fail()) {
		return nullptr;
	}

	MovesetHeader header{ 0 };
	file.read((char*)&header, sizeof(MovesetHeader));

	size_t readBytes = file.gcount();
	if (readBytes != sizeof(MovesetHeader)) {
		// Malformed file header
		return nullptr;
	}

	return new movesetInfo{
		filename,
		Helpers::getMovesetNameFromFilename(filename),
		std::string(header.origin),
		std::string(header.target_character),
		std::string(header.date)
	};
}

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

void GameExtract::ReloadMovesetList()
{
	// Loop through every file
	for (const auto& entry : std::filesystem::directory_iterator(MOVESET_DIRECTORY))
	{
		// todo: see how this works with utf8 chars
		std::string filename = entry.path().string();

		if (!Helpers::endsWith(filename, MOVESET_FILENAME_EXTENSION)) {
			// Skip files that we do not recognize
			continue;
		}

		// If new file is detected, fetch its infos
		if (!m_extractedMovesetFilenames.contains(filename))
		{
			m_extractedMovesetFilenames.insert(filename);

			movesetInfo* moveset = fetchMovesetInformations(filename);
			if (moveset == nullptr) {
				moveset = new movesetInfo{
					filename,
					Helpers::getMovesetNameFromFilename(filename),
					"INVALID",
					"",
					""
				};
			}
			extractedMovesets.push_back(moveset);
		}
	}

	// Delete entries that don't exist anymore
	for (size_t i = 0; i < extractedMovesets.size();) {

		movesetInfo* moveset = extractedMovesets[i];
		struct stat buffer;

		if ((stat(moveset->filename.c_str(), &buffer) != 0)) {
			// File does not exist anymore, de-allocate the info we stored about it
			m_extractedMovesetFilenames.erase(m_extractedMovesetFilenames.find(moveset->filename));
			delete moveset;
			extractedMovesets.erase(extractedMovesets.begin() + i, extractedMovesets.begin() + i + 1);
		}
		else {
			++i;
		}
	}
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
		ReloadMovesetList();
		if (process->IsAttached() && process->CheckRunning()) {
			LoadCharacterNames();
			while (m_playerAddress.size() > 0)
			{
				// Start extraction
				m_extractor->Extract(m_playerAddress[0], &progress);
				m_playerAddress.erase(m_playerAddress.begin());
				ReloadMovesetList();
			}
		}
		else if (currentGameId != -1) {
			// Process closed, try to attach again in case it is restarted
			if (process->Attach(currentGameProcess.c_str())) {
				OnProcessAttach();
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(GAME_INTERACTION_THREAD_SLEEP_MS));
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