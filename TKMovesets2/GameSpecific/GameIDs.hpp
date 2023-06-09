#pragma once

enum GameId_
{
	GameId_T7,
	GameId_T8,
	GameId_TTT2,
	GameId_TREV,
	GameId_T6,
	GameId_T5,
};

namespace GameVersions
{
	enum T7
	{
		NONE = 0,

		FROM_TTT2,
		FROM_TREV,
		FROM_T6,
		FROM_T5,
	};

	enum TTT2
	{
		RPCS3_BLES01702_0100 = 0
	};
};