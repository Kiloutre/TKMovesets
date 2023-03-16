#pragma once

#include <ImGui.h>
#include <string>
#include <stdint.h>
#include <vector>
#include <utility>
#include <algorithm>

#include "GameData.hpp"
#include "GameProcess.hpp"

#include "constants.h"
#include "GameAddresses.h"
#include "MovesetStructs.h"

# define FORM_INPUT_BUFSIZE (32)

// Vector with unique elements
template < typename T >
class VectorSet : public std::vector<T> {
public:
	using iterator = typename std::vector<T>::iterator;
	using value_type = typename std::vector<T>::value_type;

	std::pair<iterator, bool> push_back(const value_type& val) {
		auto it = ::std::find(this->begin(), this->end(), val);
		if (it == this->end())
			it = ::std::vector<T>::insert(this->end(), val);

		return std::pair<iterator, bool>(it, true);
	}
};

enum EditorConstants_
{
	// Marks the end of a requirements list
	EditorConstants_RequirementEnd,
	// Marks a cancel as referring to a group cancel
	EditorConstants_GroupedCancelCommand,
	// Marks the end of a cancel list
	EditorConstants_CancelCommandEnd,
	// Marks the end of a grouped cancel list
	EditorConstants_GroupedCancelCommandEnd,
};

enum EditorWindowType_
{
	EditorWindowType_Move,
	EditorWindowType_Extraproperty,
	EditorWindowType_Voiceclip,
	EditorWindowType_Cancel,
	EditorWindowType_CancelExtradata,
	EditorWindowType_GroupedCancel,
	EditorWindowType_Requirement,
	EditorWindowType_HitCondition,
	EditorWindowType_Reactions,
	EditorWindowType_Pushback,
	EditorWindowType_PushbackExtradata,
	EditorWindowType_MoveBeginProperty,
	EditorWindowType_MoveEndProperty,
};

typedef uint16_t EditorInputFlag;
enum EditorInputFlag_
{
	EditorInput_String    = (1 << 0),
	EditorInput_Clickable = (1 << 1),

	EditorInput_H64       = (1 << 2),
	EditorInput_U64       = (1 << 3),
	EditorInput_S64       = (1 << 4),
	EditorInput_U32       = (1 << 5),
	EditorInput_H32       = (1 << 6),
	EditorInput_S32       = (1 << 7),
	EditorInput_U16       = (1 << 8),
	EditorInput_H16       = (1 << 9),
	EditorInput_S16       = (1 << 10),


	EditorInput_PTR       = (EditorInput_S64 | EditorInput_Clickable),

	/*
	EditorInput_float,
	*/
};

struct EditorInput
{
	// CContains the field name in short format, used for easy-to-read checks, better than having to type the full name
	std::string name;
	// Contains the field name, used to show the correct translation string
	std::string field_fullname;
	// Use to split various fields into various tree-like categories
	uint8_t category = 0;
	// Sets the allowed charset for the input
	ImGuiInputTextFlags imguiInputFlags = 0;
	// Our own flags, required for our overflow checks
	EditorInputFlag flags = 0;
	// The string buffer containing the input text
	char buffer[FORM_INPUT_BUFSIZE] = "INVALID";
	// Contains true if the buffer contains invalid data
	bool errored = false;
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
	// Avoids having to do a std::format every frame
	std::string moveId_str;
	// The actual name of the move
	std::string name;
	// Avoids having to do a std::format every frame
	std::string alias_str;
	// Store the color to display incase of move with flags
	unsigned int color;
	// Index of the move in the movelist
	uint16_t moveId;
	// Store the alias of this move, or 0 if none1
	uint16_t aliasId;
	// Store useful flags for quick filtering / coloring
	EditorMoveFlags flags;
};

struct EditorTable
{
	std::vector<uint16_t> aliases;
	uint64_t groupCancelCount;
	/*
	uint64_t reactionsCount;
	uint64_t requirementCount;
	uint64_t hitConditionCount;
	uint64_t projectileCount;
	uint64_t pushbackCount;
	uint64_t pushbackExtradataCount;
	uint64_t cancelCount;
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
	// Used for color-coding the movelist
	unsigned int GetMoveColorFromFlag(EditorMoveFlags flags);
	// Returns a format depending on the input type
	const char* GetFieldFormat(EditorInputFlag flags);
	// Returns an imgui Input() charset depending on the input type
	ImGuiInputTextFlags GetFieldCharset(EditorInputFlag flags);
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

	// Stores a <name, offset> animation map
	std::map<std::string, gameAddr> m_animNameMap;
	// Stores the same animation map but in <offset, name> 
	std::map<gameAddr, std::string> m_animNameMap_reverse;

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
	// Constants useful constant variables, to be set on a per-game basis
	std::map<EditorConstants_, int> constants;


	// Forms
	// Returns the fields to display to build a single form of a certain type (move, voiceclip, cancel extradata, etc...)
	virtual std::map<std::string, EditorInput*> GetFormFields(EditorWindowType_ type, uint16_t id, VectorSet<std::string>& drawOrder) = 0;
	// Returns the fields to display to build a list of forms of a certain type (extraproperties, requirements, cancels, etc...)
	virtual std::vector<std::map<std::string, EditorInput*>> GetFormFieldsList(EditorWindowType_ type, uint16_t id, VectorSet<std::string>& drawOrder) = 0;
	// Returns true if the given field is valid
	virtual bool ValidateField(EditorWindowType_ fieldType, std::string fieldShortName, EditorInput* field) = 0;
	// Save a single struct in the moveset
	virtual void SaveItem(EditorWindowType_ type, uint16_t id, std::map<std::string, EditorInput*>& inputs) = 0;

	// -- Iteractons -- //
	// Sets the current move of a player
	virtual void SetCurrentMove(uint8_t playerId, gameAddr playerMoveset, size_t moveId) = 0;
};