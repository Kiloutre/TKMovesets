#pragma once

#include <string>

#include "GameIds.hpp"
#include "LocalStorage.hpp"

namespace AnimExtractor
{
	void _FromT7(const movesetInfo& mInfo);
	void _FromTTT2(const movesetInfo& mInfo);
	void _FromT6(const movesetInfo& mInfo);

	// Extracts the animation of every given movesets in appropriate folders
	// Does not extract if the moveset's animations have already been extracted (matches by original character name)
	void ExtractAnimations(const std::vector<movesetInfo> movesets)
	{
		for (auto& m : movesets)
		{	
			if (!m.is_valid) continue;

			switch (m.gameId)
			{
			case GameId_T7:
				_FromT7(m);
				break;
			case GameId_TTT2:
				_FromTTT2(m);
				break;
			case GameId_T6:
				_FromT6(m);
				break;
			}
		}
	}
};