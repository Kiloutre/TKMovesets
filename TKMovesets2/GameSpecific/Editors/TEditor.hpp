#pragma once

#include <ImGui.h>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <thread>
#include <set>

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "Helpers.hpp"
#include "BaseGameSpecificClass.hpp"
#include "Online.hpp"
#include "EditorLogic.hpp"

#include "constants.h"
#include "GameTypes.h"
#include "MovesetStructs.h"

namespace TEditorUtils
{
	unsigned int GetMoveColorFromFlag(EditorMoveFlags flags);
};

enum TEditorWindowType_
{
	// Invalid must always be zero
	TEditorWindowType_INVALID = 0,

	TEditorWindowType_Move,

	TEditorWindowType_Voiceclip,

	TEditorWindowType_HitCondition,
	TEditorWindowType_Reactions,
	TEditorWindowType_Pushback,
	TEditorWindowType_PushbackExtradata,

	TEditorWindowType_Requirement,

	TEditorWindowType_Cancel,
	TEditorWindowType_GroupedCancel,
	TEditorWindowType_CancelExtradata,

	TEditorWindowType_InputSequence,
	TEditorWindowType_Input,

	TEditorWindowType_Extraproperty,
	TEditorWindowType_MoveBeginProperty,
	TEditorWindowType_MoveEndProperty,

	TEditorWindowType_Projectile,

	TEditorWindowType_CameraData,
	TEditorWindowType_ThrowCamera,


	TEditorWindowType_MovelistDisplayable,
	TEditorWindowType_MovelistPlayable,
	TEditorWindowType_MovelistInput,


	TEditorWindowType_Invalid,
};

// Base editor class for T games
class TEditor : public EditorLogic
{
protected:
	// Stores a <name, offset> animation map
	std::map<std::string, gameAddr> m_animNameToOffsetMap;
	// Stores a <offset, offset> animation map
	std::map<gameAddr, gameAddr> m_animOffsetToNameOffset;
public:
	// If moveset possesses movelist data
	bool hasDisplayableMovelist = false;
	// Contains every move, in a displayable format. Not tied to training mode movelist.
	std::vector<DisplayableMove*> displayableMovelist;
	// Set to true when updating displayableMovelist in order to tell EditorVisuals to refresh its movelist
	bool mustReloadMovelist = false;
	// Store move ID aliases
	std::vector<uint16_t> aliases;

	TEditor(GameProcess& process, GameData& game, const GameInfo* gameInfo);
	virtual ~TEditor();


	// Return the movelist in a quickly displayable format
	virtual void ReloadDisplayableMoveList() = 0;
	// For the movelist displayed to the left
	virtual void RecomputeDisplayableMoveFlags(uint16_t moveId) = 0;
	// Returns the given player current move id
	virtual uint16_t GetCurrentMoveID(uint8_t playerId) const = 0;


	// -- Iteractions -- //
	// Sets the current move of a player
	virtual void SetCurrentMove(uint8_t playerId, gameAddr playerMoveset, size_t moveId) = 0;

	// -- Command Utils -- //
	// Returns a command string from two seperate direction & button fields
	virtual std::string GetCommandStr(const char* direction, const char* button) const = 0;
	// Returns a command string from a command field
	virtual std::string GetCommandStr(const char* commandBuf) const = 0;
	// Writes the input sequence to string, or writes to outSize if the sequence is over MAX_INPUT_SEQUENCE_SHORT_LEN inputs
	virtual void GetInputSequenceString(int id, std::string& outStr, int& outSize) const = 0;
	// Returns a command string 
	virtual std::string GetDisplayableMovelistInputStr(const char* directions, const char* buttons) const = 0;
	// Returns the color of a MvlDisplayable field according to its type
	virtual int GetDisplayableMovelistEntryColor(EditorInput* field) const = 0;
	// Returns true if a displayable entry in the movelist is a combo
	virtual bool IsMovelistDisplayableEntryCombo(EditorInput* field) const = 0;
	// Returns true if a displayable entry in the movelist is a category (don't increment index)
	virtual	bool IsMovelistDisplayableEntryCategory(EditorInput* field) const = 0;

	// -- Utils -- //
	// Returns true if the command is an input sequence
	virtual bool IsCommandInputSequence(const char* buffer) const = 0;
	// Determines if a command states that the cancel refers to a grouped cancel 
	virtual bool IsCommandGroupedCancelReference(const char* buffer) const = 0;
	// Returns the ID of an input sequence from a (grouped)cancel's command buffer
	virtual int GetCommandInputSequenceID(const char* buffer) const = 0;
	// Returns true if the property ID refers to a throw camera
	virtual bool IsPropertyThrowCameraRef(const char* buffer) const = 0;
	// Returns true if the property ID refers to a projectile
	virtual bool IsPropertyProjectileRef(const char* buffer) const = 0;
	// Returns true if the voiceclip value indicates the end of the voiceclip list
	virtual bool IsVoicelipValueEnd(const char* buffer) const = 0;
	// Returns the amount of structure in the given structure list type (move, cancel etc)
	virtual unsigned int GetStructureCount(EditorWindowType type) const = 0;
	// Returns the amount of MOTA animations inside of a specific MOTA
	virtual unsigned int GetMotaAnimCount(int motaId) const = 0;
	// Returns a string computed based on a movelist displayable's icons value and more
	virtual std::string GetMovelistDisplayableLabel(InputMap& fieldMap) const = 0;
	// Returns the list of inputs in the displayable movelist
	virtual unsigned int GetMovelistDisplayableInputCount() const = 0;
	// Returns a move's name from its ID
	virtual const char* GetMoveName(uint32_t moveid) const = 0;

	virtual void ExecuteExtraprop(EditorInput* idField, EditorInput* valueField) {};

	// -- References -- //

	struct CancelProjectileReference {
		struct Reference {
			unsigned int move_id;
			std::string name;
		};
		unsigned int id;
		std::string id_str;
		std::string references_count_str;
		std::vector<Reference> moves_references;
	};

	struct CancelMoveReference {
		unsigned int move_id;
		std::string name;
	};

	struct MoveCancelReference {
		struct Reference {
			unsigned int move_id;
			std::string name;
		};

		struct Requirement {
			unsigned int id;
			unsigned int value;
		};

		unsigned int id;
		unsigned int list_start_id;
		std::string list_start_id_str;
		std::string command_str;

		std::string detection_start;
		std::string detection_end;
		std::string starting_frame;

		std::vector<Requirement> conditions;
		std::string conditions_str;

		std::vector<Reference> move_references;
		std::vector<unsigned int> projectile_references;
	};

	struct MoveGroupedCancelReference {
		struct CancelReference {
			unsigned int id;
			unsigned int list_start_id;
			std::string list_start_id_str;

			std::string starting_frame;
		};

		struct Reference {
			unsigned int move_id;
			std::string name;
		};

		struct Requirement {
			unsigned int id;
			unsigned int value;
		};

		unsigned int id;
		unsigned int list_start_id;
		std::string list_start_id_str;
		std::string command_str;

		std::string detection_start;
		std::string detection_end;
		std::string starting_frame;

		std::vector<Requirement> conditions;
		std::string conditions_str;

		std::vector<Reference> move_references;
		std::vector<unsigned int> projectile_references;
		std::vector<CancelReference> cancel_references;
	};

	struct MoveReactionsReference {
		struct Reference {
			unsigned int move_id;
			std::string name;
		};
		unsigned int id;
		std::string id_str;
		std::vector<unsigned int> situation_ids;
		std::string situations_str;
		std::vector<Reference> references;
		std::string references_count_str;
	};

	virtual std::vector<TEditor::CancelProjectileReference> ListCancelsProjectilesReferences(unsigned int cancel_start_id) const = 0;
	virtual std::vector<TEditor::CancelMoveReference> ListCancelsMoveReferences(unsigned int cancel_start_id) const = 0;
	virtual std::vector<MoveCancelReference> ListMoveCancelsReferences(unsigned int moveid) const = 0;
	virtual std::vector<MoveGroupedCancelReference> ListMoveGroupedCancelsReferences(unsigned int moveid) const = 0;
	virtual std::vector<MoveReactionsReference> ListMoveReactionsReferences(unsigned int moveid) const = 0;
};