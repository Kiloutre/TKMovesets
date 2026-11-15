#pragma once
#include <cstdint>

// Items here may only be renamed, not moved / deleted / replaced
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
	namespace T8
	{
		inline constexpr uint16_t FROM_T7 = 1; // conversion_origin, not minorVersion
		enum
		{
			CLOSED_NETWORK_TEST,
			CLOSED_BETA_TEST,
			RETAIL, // Append only: CNT/CBT values are serialized in existing files.
		};
	}

	namespace T7
	{
		enum
		{
			NONE = 0,

			FROM_TTT2,
			FROM_TREV,
			FROM_T6,
			FROM_T5,
			FROM_T5DR,
			FROM_T8, // retail Tekken 8 (T8ToT7)
		};
	}

	namespace TTT2
	{
		enum
		{
			RPCS3_BLES01702_0100 = 0,
			RPCS3_BLES01702_0103 = 1,
		};
	}

	namespace T6
	{
		enum
		{
			RPCS3_BLES00635 = 0,
			PPSSPP_ULUS10466,
			RPCS3_BLUS30369,
		};
	}

	namespace T5
	{
		enum
		{
			DEFAULT = 0,

			DR_ONLINE
		};
	}
};
