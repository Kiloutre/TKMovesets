#pragma once

#include <string>
#include <stdint.h>

#include "GameData.hpp"
#include "GameProcess.hpp"

#include "constants.h"
#include "GameAddresses.h"
#include "MovesetStructs.h"

typedef uint16_t EditorMoveFlags;
enum EditorMoveFlags_
{
	EditorMoveFlags_Attack = (1 << 0),
	EditorMoveFlags_OtherAttack = (1 << 1),
	EditorMoveFlags_Generic = (1 << 2),
	EditorMoveFlags_Throw = (1 << 3),
	EditorMoveFlags_ThrowReaction = (1 << 4),
	EditorMoveFlags_Custom = (1 << 5),
};


struct DisplayableMove
{
	uint16_t moveId;
	std::string name;
	// Store the alias of this move, or 0 if none1
	uint16_t aliasId;
	// Store useful flags for quick filtering / coloring
	EditorMoveFlags flags;
};

struct EditorTable
{
	std::vector<uint16_t> aliases;
};

class DLLCONTENT Editor
{
protected:
	// Access the game's data through here. Use for live edition and live data querying only, not import
	GameProcess* m_process = nullptr;
	// Access the game's data through here. Use for live edition and live data querying only, not import
	GameData* m_game = nullptr;
	// Contains our header data
	TKMovesetHeader* m_header = nullptr;

	// Contains the moveset, including our own header.
	Byte* m_moveset = nullptr;
	// Contains the size of the moveset, including our own header
	uint64_t m_movesetSize = 0;
	// Contains the moveset, without our header
	byte* m_movesetData = nullptr;

public:
	Editor(GameProcess* process, GameData* game) : m_process(process), m_game(game) {}

	//
	virtual void LoadMoveset(Byte* t_moveset, uint64_t t_movesetSize) = 0;
	//
	virtual EditorTable GetMovesetTable() = 0;
	// Return the movelist in a quickly displayable format
	virtual std::vector<DisplayableMove*> GetDisplayableMoveList() = 0;
	//
	//AddMove();
};