#pragma once

#include <ImGui.h>
#include <string>
#include <stdint.h>

#include "GameData.hpp"
#include "GameProcess.hpp"

#include "constants.h"
#include "GameAddresses.h"
#include "MovesetStructs.h"

typedef uint16_t EditorInputType;
enum EditorInputType_
{
	EditorInputType_signed,
	EditorInputType_unsigned,
	EditorInputType_hexnumber,

	EditorInputType_float,
	EditorInputType_negativeFloat,
	EditorInputType_bool,
	EditorInputType_text,
};

struct EditorInput
{
	std::string name;
	uint16_t offset;
	uint8_t memberSize;
	uint8_t category = 0;
	ImGuiInputTextFlags imguiInputFlags = 0;
	EditorInputType flags = 0;
	char buffer[32] = "";
};

// Pre-calculate these flags in order to color-code, sort and filter the movelist
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

// Used for the move list
struct DisplayableMove
{
	uint16_t moveId;
	std::string name;
	// Store the alias of this move, or 0 if none1
	uint16_t aliasId;
	// Store useful flags for quick filtering / coloring
	EditorMoveFlags flags;
	// Store the color to display incase of move with flags
	unsigned int color;
};

struct EditorTable
{
	std::vector<uint16_t> aliases;
	/*
	uint64_t reactionsCount;
	uint64_t requirementCount;
	uint64_t hitConditionCount;
	uint64_t projectileCount;
	uint64_t pushbackCount;
	uint64_t pushbackExtradataCount;
	uint64_t cancelCount;
	uint64_t groupCancelCount;
	uint64_t cancelExtradataCount;
	uint64_t extraMovePropertyCount;
	uint64_t moveBeginningPropCount;
	uint64_t moveEndingPropCount;
	uint64_t moveCount;
	uint64_t voiceclipCount;
	uint64_t inputSequenceCount;
	uint64_t inputCount;
	uint64_t unknownParryRelatedCount;
	uint64_t cameraDataCount;
	uint64_t throwsCount;
	*/
};


namespace EditorUtils
{
	unsigned int GetMoveColorFromFlag(EditorMoveFlags flags);
}

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

	// Loads important moveset data in our class, required to start functionning
	virtual void LoadMoveset(Byte* t_moveset, uint64_t t_movesetSize) = 0;
	// Returns useful informations contained within the moveset  actual header
	virtual EditorTable GetMovesetTable() = 0;
	// Return the movelist in a quickly displayable format
	virtual std::vector<DisplayableMove*> GetDisplayableMoveList() = 0;
	// Returns the given player current move id
	virtual uint16_t GetCurrentMoveID(uint8_t playerId) = 0;
	// Returns a list of inputs to build forms with
	virtual std::vector<EditorInput*> GetFormInputs(std::string identifier) = 0;
	// Fill a field with a value related to the identifier and id
	virtual void FillField(std::string identifier, EditorInput* field, uint32_t id) = 0;
	// Returns true if the given field is valid
	virtual bool ValidateField(EditorInput* field) = 0;
};