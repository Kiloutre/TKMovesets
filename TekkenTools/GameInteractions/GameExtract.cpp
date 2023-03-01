#include <set>
#include <chrono>
#include <thread>
#include <iostream>
#include <filesystem>

#include "GameExtract.hpp"
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

	if ( Helpers::isHeaderStringMalformated(header.origin, sizeof(header.origin)) ||
		Helpers::isHeaderStringMalformated(header.origin, sizeof(header.target_character)) ||
		Helpers::isHeaderStringMalformated(header.origin, sizeof(header.version_string)) ||
		Helpers::isHeaderStringMalformated(header.origin, sizeof(header.date)) ) {
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
	// Start the extraction thread that will run regularly, attach itself to the game whenever found and consume the queue
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

	// Delete moveset entries that don't exist anymore (deleted files)
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
	if (m_extractor != nullptr && m_extractor->CanExtract()) {
		gameAddr playerAddress = game->ReadPtr("p1_addr");
		gameAddr playerStructSize = GameAddressesFile::GetSingleValue("val_playerstruct_size");

		for (int playerId = 0; playerId < characterCount; ++playerId) {
			characterNames[playerId] = m_extractor->GetPlayerCharacterName(playerAddress + playerId * playerStructSize);
		}
	}
}

void GameExtract::Update()
{
	// Executed in its own thread, is the one in charge of extraction, that way it won't interrupt the GUI rendering
	while (m_threadStarted)
	{
		// Update the list of moveset files. This is better than doing it in the display thread
		ReloadMovesetList();

		// Ensure the process is still open and valid before possibly extracting
		if (process->IsAttached() && process->CheckRunning()) {
			// Load character name for fancy live displaying : You know who you're extracting
			LoadCharacterNames();

			// Extraction queue is a FIFO (first in first out) queue
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
	// Load the appropriate extractor for the selected game and attempt to load the character names
	InstantiateExtractor();
	LoadCharacterNames();
}

void GameExtract::InstantiateExtractor()
{
	if (m_extractor != nullptr) {
		delete m_extractor;
	}

	// Every game has its own extraction subtleties so we use polymorphism to manage that

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

	if (process->IsAttached()) {
		process->Detach();
	}

	currentGameProcess = std::string(processName);
	currentGameId = gameId;
	if (process->Attach(processName)) {
		OnProcessAttach();
	}
}

bool GameExtract::CanExtract()
{
	if (m_extractor == nullptr) {
		return false;
	}
	// Per-game definition
	return m_extractor->CanExtract();
}

bool GameExtract::IsBusy()
{
	// There are still playerAddresss to extract from
	return m_playerAddress.size() > 0;
}

void GameExtract::QueueCharacterExtraction(int playerId)
{
	// It is safe to call this function even while an extraction is ongoing
	gameAddr playerAddress = game->ReadPtr("p1_addr");
	gameAddr playerStructSize = GameAddressesFile::GetSingleValue("val_playerstruct_size");

	if (playerId == -1) {
		// Queue the extraction of every character one by one
		for (playerId = 0; playerId < characterCount; ++playerId) {
			OrderExtraction(playerAddress + playerId * playerStructSize);
		}
	}
	else {
		// Queue the extraction of one character
		OrderExtraction(playerAddress + playerId * playerStructSize);
	}
}