#include "Games.hpp"
#include "Extractor_t7.hpp"
#include "Importer_t7.hpp"

// You should never reorder the list because it would change the game ids which are contained within movesets
// THe IDs correspond to the index of the game in the list
const GameInfo cg_gamesInfo[] = {
	{
		.name = "Tekken 7",
		.processName = "TekkenGame-Win64-Shipping.exe",
		.characterCount = 2,
		.flags = GameMovesetEdition,
		.extractor = new FactoryType<ExtractorT7>,
		.importer = new FactoryType<ImporterT7>,
	},
	{
		.name = "Tekken 8",
		.processName = "Tekken8.exe",
		.characterCount = 2,
		.flags = 0,
		.extractor = new FactoryType<ExtractorT7>,
		.importer = nullptr
	},
	{
		.name = "Tekken Tag 2",
		.processName = "Cemu.exe",
		.characterCount = 2,
		.flags = 0,
		.extractor = nullptr,
		.importer = new FactoryType<ImporterT7>,
	},
};

namespace Games
{
	GameInfo* GetGameInfo(uint8_t idx)
	{
		return (GameInfo*)&cg_gamesInfo[idx];
	}

	uint8_t GetGamesCount()
	{
		return sizeof(cg_gamesInfo) / sizeof(*cg_gamesInfo);
	}

	Extractor* FactoryGetExtractor(uint8_t gameId, GameProcess* process, GameData* game)
	{
		return (Extractor*)cg_gamesInfo[gameId].extractor->allocate(process, game);
	}

	Importer* FactoryGetImporter(uint8_t gameId, GameProcess* process, GameData* game)
	{
		return (Importer*)cg_gamesInfo[gameId].importer->allocate(process, game);
		return nullptr;
	}
};