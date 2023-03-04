#include "Games.hpp"
#include "Extractor_t7.hpp"
#include "Importer_t7.hpp"

// Should respect the indexes showed in GameId enum
const GameInfo cg_gamesInfo[] = {
	{
		// 	GameId_t7 = 0,
		.name = "Tekken 7",
		.processName = "TekkenGame-Win64-Shipping.exe",
		.characterCount = 2,
		.flags = GameExtractable | GameImportable | GameCamera
	},
	{
		// 	GameId_t8 = 1,
		.name = "Tekken 8",
		.processName = "Tekken8.exe",
		.characterCount = 2,
		.flags = GameExtractable | GameImportable | GameCamera
	},
	{
		// 	GameId_t8 = 2,
		.name = "Tekken Tag 2",
		.processName = ".exe",
		.characterCount = 2,
		.flags = GameExtractable
	},
};

namespace Games
{
	GameInfo* GetGameInfo(uint8_t idx)
	{
		return (GameInfo*)&cg_gamesInfo[idx];
	}

	size_t GetGamesCount()
	{
		return sizeof(cg_gamesInfo) / sizeof(*cg_gamesInfo);
	}


	Extractor* FactoryGetExtractor(uint8_t gameId, GameProcess* process, GameData* game)
	{
		switch (gameId)
		{
		case GameId_t7:
			return new ExtractorT7(process, game);
			break;
		}
		return nullptr;
	}

	Importer* FactoryGetImporter(uint8_t gameId, GameProcess* process, GameData* game)
	{
		switch (gameId)
		{
		case GameId_t7:
			return new ImporterT7(process, game);
			break;
		}
		return nullptr;
	}
};