#include "Games.hpp"

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
};