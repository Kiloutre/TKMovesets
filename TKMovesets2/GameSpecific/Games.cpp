#include "Games.hpp"

#include "Extractor_t7.hpp"
#include "Importer_t7.hpp"
#include "Editor_t7.hpp"
#include "Online_t7.hpp"

// You should never reorder the list because it would change the game ids which are contained within movesets
// THe IDs correspond to the index of the game in the list
const GameInfo cg_gamesInfo[] = {
	{
		.name = "Tekken 7",
		.processName = "TekkenGame-Win64-Shipping.exe",
		.movesetNamePrefix = "T7_",
		.gameId = GameId_T7,
		.minorVersion = 0,
		.characterCount = 2,
		.flags = GameFlag_MovesetLiveEditable,
		.dataString = "t7",
		.minorDataString = "t7",
		.supportedImports = {},
		.extractor = new FactoryType<ExtractorT7>,
		.importer = new FactoryType<ImporterT7>,
		.editor = new FactoryType<EditorT7>,
		.onlineHandler = new FactoryType<OnlineT7>
	},
	{
		.name = "Tekken 8",
		.processName = "Tekken8.exe",
		.movesetNamePrefix = "T8_",
		.gameId = GameId_T8,
		.minorVersion = 0,
		.characterCount = 2,
		.flags = 0,
		.dataString = "t8",
		.minorDataString = "t8",
		.supportedImports = { GameId_T7 },
		.extractor = nullptr,
		.importer = nullptr,
		.editor = nullptr,
		.onlineHandler = nullptr,
	},
	{
		.name = "Tekken Tag 2",
		.processName = "Cemu.exe",
		.movesetNamePrefix = "TTT2_",
		.gameId = GameId_TTT2,
		.minorVersion = 0,
		.characterCount = 2,
		.flags = 0,
		.dataString = "tag2",
		.minorDataString = "tag2",
		.supportedImports = {},
		.extractor = nullptr,
		.importer = nullptr,
		.editor = nullptr,
		.onlineHandler = nullptr,
	},
};

namespace Games
{
	const GameInfo* GetGameInfoFromIndex(unsigned int idx)
	{
		return (GameInfo*)&cg_gamesInfo[idx];
	}

	const GameInfo* GetGameInfoFromIdentifier(uint16_t gameId)
	{
		for (size_t i = 0; i < _countof(cg_gamesInfo); ++i)
		{
			if (cg_gamesInfo[i].gameId == gameId)
			{
				return &cg_gamesInfo[i];
			}
		}
		return nullptr;
	}

	const GameInfo* GetGameInfoFromIdentifier(uint16_t gameId, uint16_t minorVersion)
	{
		for (size_t i = 0; i < _countof(cg_gamesInfo); ++i)
		{
			if (cg_gamesInfo[i].gameId == gameId && cg_gamesInfo[i].minorVersion == minorVersion)
			{
				return &cg_gamesInfo[i];
			}
		}
		return nullptr;
	}

	unsigned int GetGamesCount()
	{
		return (int)_countof(cg_gamesInfo);
	}

	//

	unsigned int GetExtractableGamesCount()
	{
		unsigned int count = 0;
		for (size_t i = 0; i < _countof(cg_gamesInfo); ++i)
		{
			if (cg_gamesInfo[i].extractor != nullptr) {
				++count;
			}
		}
		return count;
	}

	unsigned int GetImportableGamesCount()
	{
		unsigned int count = 0;
		for (size_t i = 0; i < _countof(cg_gamesInfo); ++i)
		{
			if (cg_gamesInfo[i].importer != nullptr) {
				++count;
			}
		}
		return count;
	}

	//

	bool IsGameEditable(uint16_t gameId, uint16_t minorVersion)
	{
		auto game = GetGameInfoFromIdentifier(gameId, minorVersion);
		return game && game->editor != nullptr;
	}

	bool IsGameLiveEditable(uint16_t gameId, uint16_t minorVersion)
	{
		auto game = GetGameInfoFromIdentifier(gameId, minorVersion);
		return game && (game->flags & GameFlag_MovesetLiveEditable) > 0;
	}

	//
	
	Extractor* FactoryGetExtractor(const GameInfo* gameInfo, GameProcess* process, GameData* game)
	{
		Extractor* ex = (Extractor*)gameInfo->extractor->allocate(process, game, gameInfo);
		ex->characterCount = gameInfo->characterCount;
		return ex;
	}

	Importer* FactoryGetImporter(const GameInfo* gameInfo, GameProcess* process, GameData* game)
	{
		Importer* im = (Importer*)gameInfo->importer->allocate(process, game, gameInfo);
		im->characterCount = gameInfo->characterCount;
		return im;
	}

	Editor* FactoryGetEditor(const GameInfo* gameInfo, GameProcess* process, GameData* game)
	{
		Editor* ed = (Editor*)gameInfo->editor->allocate(process, game, gameInfo);
		// No need to store character count here because the editor is used alongside the importer which stores that information already
		return ed;
	}

	Online* FactoryGetOnline(const GameInfo* gameInfo, GameProcess* process, GameData* game)
	{
		Online* on = (Online*)gameInfo->onlineHandler->allocate(process, game, gameInfo);
		return on;
	}
};