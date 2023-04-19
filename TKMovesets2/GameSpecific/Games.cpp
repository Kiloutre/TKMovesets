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
		.gameId = GameId_T7,
		.minorVersion = 0,
		.characterCount = 2,
		.flags = GameFlag_MovesetLiveEditable,
		.dataString = "t7",
		.extractor = new FactoryType<ExtractorT7>,
		.importer = new FactoryType<ImporterT7>,
		.editor = new FactoryType<EditorT7>,
		.onlineHandler = new FactoryType<OnlineT7>
	},
	{
		.name = "Tekken 8",
		.processName = "Tekken8.exe",
		.gameId = GameId_T8,
		.minorVersion = 0,
		.characterCount = 2,
		.flags = 0,
		.dataString = "t8",
		.extractor = nullptr,
		.importer = nullptr,
		.editor = nullptr,
		.onlineHandler = nullptr
	},
	{
		.name = "Tekken Tag 2",
		.processName = "Cemu.exe",
		.gameId = GameId_TTT2,
		.minorVersion = 0,
		.characterCount = 2,
		.flags = 0,
		.dataString = "tag2",
		.extractor = nullptr,
		.importer = nullptr,
		.editor = nullptr,
		.onlineHandler = nullptr
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
		return GetGameInfoFromIdentifier(gameId, minorVersion)->editor != nullptr;
	}

	bool IsGameLiveEditable(uint16_t gameId, uint16_t minorVersion)
	{
		return (GetGameInfoFromIdentifier(gameId, minorVersion)->flags & GameFlag_MovesetLiveEditable) > 0;
	}

	//
	
	Extractor* FactoryGetExtractor(uint16_t gameId, GameProcess* process, GameData* game)
	{
		Extractor* ex = (Extractor*)cg_gamesInfo[gameId].extractor->allocate(process, game);
		ex->characterCount = cg_gamesInfo[gameId].characterCount;
		return ex;
	}

	Importer* FactoryGetImporter(uint16_t gameId, GameProcess* process, GameData* game)
	{
		Importer* im = (Importer*)cg_gamesInfo[gameId].importer->allocate(process, game);
		im->characterCount = cg_gamesInfo[gameId].characterCount;
		return im;
	}

	Editor* FactoryGetEditor(uint16_t gameId, GameProcess* process, GameData* game)
	{
		Editor* ed = (Editor*)cg_gamesInfo[gameId].editor->allocate(process, game);
		// No need to store character count here because the editor is used alongside the importer which stores that information already
		return ed;
	}

	Online* FactoryGetOnline(uint16_t gameId, GameProcess* process, GameData* game)
	{
		Online* on = (Online*)cg_gamesInfo[gameId].onlineHandler->allocate(process, game);
		return on;
	}
};