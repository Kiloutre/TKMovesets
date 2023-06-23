#pragma once

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "Importer.hpp"
#include "Extractor.hpp"
#include "EditorLogic.hpp"
#include "EditorVisuals/EditorVisuals.hpp"
#include "Online.hpp"
#include "GameInfo.hpp"

#include "constants.h"
#include "GameIDs.hpp"

namespace Games
{
	// Returns a pointer to a struct containing a game's name, process name, character count
	const GameInfo* GetGameInfoFromIndex(unsigned int index);
	const GameInfo* GetGameInfoFromIdentifier(uint16_t gameId);
	const GameInfo* GetGameInfoFromIdentifier(uint16_t gameId, uint16_t minorVersion);

	// Returns the amount of supported games
	unsigned int GetGamesCount();
	// Returns the amount of games that upport extraction
	unsigned int GetExtractableGamesCount();
	// Returns the amount of games that upport importation
	unsigned int GetImportableGamesCount();

	// Returns true if moveset edition on the moveset is allowed
	bool IsGameEditable(uint16_t gameId, uint16_t minorVersion);
	// Returns true if live moveset edition on the moveset is allowed
	bool IsGameLiveEditable(uint16_t gameId, uint16_t minorVersion);

	// Instantiate a new game-specific extractor
	Extractor* FactoryGetExtractor(const GameInfo* gameInfo, GameProcess& process, GameData& game);

	// Instantiate a new game-specific importer
	Importer* FactoryGetImporter(const GameInfo* gameInfo, GameProcess& process, GameData& game);

	// Instantiate a new game-specific editor
	EditorLogic* FactoryGetEditorLogic(const GameInfo* gameInfo, GameProcess& process, GameData& game);
	EditorVisuals* FactoryGetEditorVisuals(const GameInfo* gameInfo, const movesetInfo* movesetInfos, GameAddressesFile* addrFile, LocalStorage* storage);

	// Instantiate a new game-specific online handler
	Online* FactoryGetOnline(const GameInfo* gameInfo, GameProcess& process, GameData& game);
}