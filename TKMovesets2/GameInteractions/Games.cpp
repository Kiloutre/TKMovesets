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
		.flags = GameFlag_MovesetEditable,
		.extractor = new FactoryType<ExtractorT7>,
		.importer = new FactoryType<ImporterT7>,
	},
	{
		.name = "Tekken 8",
		.processName = "Tekken8.exe",
		.characterCount = 2,
		.flags = 0,
		.extractor = nullptr,
		.importer = nullptr
	},
	{
		.name = "Tekken Tag 2",
		.processName = "Cemu.exe",
		.characterCount = 2,
		.flags = 0,
		.extractor = nullptr,
		.importer = nullptr,
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

	size_t GetExtractableGamesCount()
	{
		size_t count = 0;
		for (size_t i = 0; i < sizeof(cg_gamesInfo) / sizeof(*cg_gamesInfo); ++i)
		{
			if (cg_gamesInfo[i].extractor != nullptr) {
				++count;
			}
		}
		return count;
	}

	size_t GetImportableGamesCount()
	{
		size_t count = 0;
		for (size_t i = 0; i < sizeof(cg_gamesInfo) / sizeof(*cg_gamesInfo); ++i)
		{
			if (cg_gamesInfo[i].importer != nullptr) {
				++count;
			}
		}
		return count;
	}

	Extractor* FactoryGetExtractor(uint8_t gameId, GameProcess* process, GameData* game)
	{
		Extractor* ex = (Extractor*)cg_gamesInfo[gameId].extractor->allocate(process, game);
		ex->characterCount = cg_gamesInfo[gameId].characterCount;
		return ex;
	}

	Importer* FactoryGetImporter(uint8_t gameId, GameProcess* process, GameData* game)
	{
		Importer* im = (Importer*)cg_gamesInfo[gameId].importer->allocate(process, game);
		im->characterCount = cg_gamesInfo[gameId].characterCount;
		return im;
	}
};