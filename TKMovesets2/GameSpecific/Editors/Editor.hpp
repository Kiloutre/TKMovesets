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

#include "constants.h"
#include "GameTypes.h"
#include "MovesetStructs.h"

# define FORM_BUFSIZE (32) // Regular fields
# define FORM_STRING_BUFSIZE (64) // String fields
# define FORM_INPUT_MAX_BUFSIZE (256) // Some very long fields like displayable movelist use this
# define MAX_INPUT_SEQUENCE_SHORT_LEN (15)

// Shifting logic for list resizing
# define MUST_SHIFT_ID(id, valueChange, listStart, listOldEnd) (((int)id >= (int)listOldEnd) || (valueChange < 0 && ((int)id - valueChange) >= (int)listOldEnd))

// Animation extraction status
enum AnimExtractionStatus_
{
	AnimExtractionStatus_NotStarted,
	AnimExtractionStatus_Started,
	AnimExtractionStatus_Failed,
	AnimExtractionStatus_Finished,
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
	// Marks an entry that applies instantly in the extra property list
	EditorConstants_ExtraProperty_Instant,
	// Marks the end of an extra property list
	EditorConstants_ExtraPropertyEnd,
	// Marks that the cancel command refers to an input sequence
	EditorConstants_InputSequenceCommandStart
};

enum EditorWindowType_
{
	EditorWindowType_Move,

	EditorWindowType_Voiceclip,

	EditorWindowType_HitCondition,
	EditorWindowType_Reactions,
	EditorWindowType_Pushback,
	EditorWindowType_PushbackExtradata,

	EditorWindowType_Requirement,

	EditorWindowType_Cancel,
	EditorWindowType_GroupedCancel,
	EditorWindowType_CancelExtradata,

	EditorWindowType_InputSequence,
	EditorWindowType_Input,

	EditorWindowType_Extraproperty,
	EditorWindowType_MoveBeginProperty,
	EditorWindowType_MoveEndProperty,

	EditorWindowType_Projectile,

	EditorWindowType_CameraData,
	EditorWindowType_ThrowCamera,


	EditorWindowType_MovelistDisplayable,
	EditorWindowType_MovelistPlayable,
	EditorWindowType_MovelistInput,


	EditorWindowType_Invalid,
};

typedef uint32_t EditorInputFlag;
enum EditorInputFlag_
{
	EditorInput_String = (1 << 0),
	EditorInput_Clickable = (1 << 1),
	// Clickable regardless of field validity
	EditorInput_ClickableAlways = (1 << 2),

	EditorInput_H64 = (1 << 3),
	EditorInput_U64 = (1 << 4),
	EditorInput_S64 = (1 << 5),
	EditorInput_H32 = (1 << 6),
	EditorInput_U32 = (1 << 7),
	EditorInput_S32 = (1 << 8),
	EditorInput_H16 = (1 << 9),
	EditorInput_U16 = (1 << 10),
	EditorInput_S16 = (1 << 11),
	EditorInput_H8 = (1 << 12),
	EditorInput_S8 = (1 << 13),
	EditorInput_U8 = (1 << 14),

	EditorInput_Float = (1 << 15),

	// Used for fancy data-switch behaviour (signed -> unsigned -> hex -> signed...)
	EditorInput_DataChangeable = (1 << 16),

	// Shortcuts
	EditorInput_PTR = (EditorInput_S64 | EditorInput_Clickable),

	EditorInput_H64_Changeable = EditorInput_H64 | EditorInput_DataChangeable,
	EditorInput_U64_Changeable = EditorInput_U64 | EditorInput_DataChangeable,
	EditorInput_S64_Changeable = EditorInput_S64 | EditorInput_DataChangeable,
	EditorInput_U32_Changeable = EditorInput_U32 | EditorInput_DataChangeable,
	EditorInput_H32_Changeable = EditorInput_H32 | EditorInput_DataChangeable,
	EditorInput_S32_Changeable = EditorInput_S32 | EditorInput_DataChangeable,
	EditorInput_U16_Changeable = EditorInput_U16 | EditorInput_DataChangeable,
	EditorInput_H16_Changeable = EditorInput_H16 | EditorInput_DataChangeable,
	EditorInput_S16_Changeable = EditorInput_S16 | EditorInput_DataChangeable,
	EditorInput_U8_Changeable = EditorInput_U8 | EditorInput_DataChangeable,
	EditorInput_H8_Changeable = EditorInput_H8 | EditorInput_DataChangeable,
	EditorInput_S8_Changeable = EditorInput_S8 | EditorInput_DataChangeable,

	// Used internally for conditions
	EditorInput_Unsigned = (EditorInput_U64 | EditorInput_U32 | EditorInput_U16 | EditorInput_U8),
	EditorInput_Signed = (EditorInput_S64 | EditorInput_S32 | EditorInput_S16 | EditorInput_S8),
	EditorInput_Hex = (EditorInput_H64 | EditorInput_H32 | EditorInput_H16 | EditorInput_H8),
	EditorInput_Interactable = (EditorInput_Clickable | EditorInput_ClickableAlways),
	EditorInput_64b = (EditorInput_U64 | EditorInput_S64 | EditorInput_H64),
	EditorInput_32b = (EditorInput_U32 | EditorInput_S32 | EditorInput_H32 | EditorInput_Float),
	EditorInput_16b = (EditorInput_U16 | EditorInput_S16 | EditorInput_H16),
	EditorInput_8b = (EditorInput_U8 | EditorInput_S8 | EditorInput_H8),

};

struct EditorInput
{
	// Contains the field name in short format, used for easy-to-read checks, better than having to type the full name
	FasterStringComp name;
	// Contains the field name, used to show the correct translation string
	std::string fullName;
	// Contains the actual string that will be displayed in the form. Write on this freely.
	std::string displayName;
	// Use to split various fields into various tree-like categories
	uint8_t category = 0;
	// Sets the allowed charset for the input
	ImGuiInputTextFlags imguiInputFlags = 0;
	// Our own flags, required for our overflow checks
	EditorInputFlag flags = 0;
	// The string buffer containing the input text
	char* buffer = nullptr;
	unsigned int bufsize = 0;
	// Sets the BG color of the input
	int color = 0;
	// Contains half of the text size, used for alignment
	float textSizeHalf = 0;
	// If set to false, the field and its label will not render
	bool visible = true;
	// Contains true if the buffer contains invalid data
	bool errored = false;
	// Used to implement pasting from clipboard. Can't write into a focused field.
	std::string nextValue;
};

// Pre-calculate these flags in order to color-code, sort and filter the movelist
typedef uint16_t EditorMoveFlags;
enum EditorMoveFlags_
{
	EditorMoveFlags_Attack = (1 << 0),
	EditorMoveFlags_OtherAttack = (1 << 1),
	EditorMoveFlags_Generic = (1 << 2),
	EditorMoveFlags_CurrentGeneric = (1 << 3),
	EditorMoveFlags_Throw = (1 << 4),
	EditorMoveFlags_ThrowReaction = (1 << 5),
	EditorMoveFlags_Custom = (1 << 6),
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
};

typedef std::map<FasterStringComp, EditorInput*> InputMap;

namespace EditorUtils
{
	// Used for color coding input fields
	void SetInputfieldColor(EditorInput* field);
	// Used for color-coding the movelist
	unsigned int GetMoveColorFromFlag(EditorMoveFlags flags);
	// Returns a format depending on the input type
	constexpr const char* GetFieldFormat(EditorInputFlag flags);
	// Returns an imgui Input() charset depending on the input type
	ImGuiInputTextFlags GetFieldCharset(EditorInputFlag flags);
	// Returns an field value, parsing it as a hex or regular int depending on the type
	uint64_t GetFieldValue(EditorInput* field);
	// Sets a struct member value according to a field's buffer, parsing it correctly according to its type
	void SetMemberValue(void* memberPtr, EditorInput* field);
	// Changes between unsigned, signed AND hex data type if the field allows it
	void ChangeFieldDataType(EditorInput* field);
	// Writes the fullname to every field in a field map
	void WriteFieldFullname(InputMap& inputMap, const std::string& baseIdentifier);

	template <typename T>
	EditorInput* CreateField(const std::string& fieldName, VectorSet<std::string>& drawOrder, InputMap& inputMap, uint8_t category, uint32_t flags, T value, uint32_t bufsize = 0)
	{
		drawOrder.push_back(fieldName);

		if (bufsize == 0)
		{
			if constexpr (std::is_same_v< std::remove_cv_t<T>, char*>) {
				bufsize = FORM_STRING_BUFSIZE;
				flags |= EditorInput_String;
			}
			else {
				bufsize = FORM_BUFSIZE;
			}
		}

		EditorInput* newField = new EditorInput{
			.name = fieldName,
			.category = category,
			.imguiInputFlags = EditorUtils::GetFieldCharset(flags),
			.flags = flags,
			.buffer = new char[bufsize],
			.bufsize = bufsize,
		};

		inputMap[fieldName] = newField;
		EditorUtils::SetInputfieldColor(newField);
		sprintf_s(newField->buffer, newField->bufsize, EditorUtils::GetFieldFormat(flags), value);

		return newField;
	}
}

class DLLCONTENT Editor : public BaseGameSpecificClass
{
protected:
	// Contains our header data
	TKMovesetHeader* m_header = nullptr;
	// Store move ID aliases
	std::vector<uint16_t>* m_aliases = nullptr;

	// Contains the moveset, including our own header.
	Byte* m_moveset = nullptr;
	// Contains the size of the moveset, including our own header
	uint64_t m_movesetSize = 0;
	// Contains the moveset, without our header
	Byte* m_movesetData = nullptr;
	// Contains the moveset size, without our header
	uint64_t m_movesetDataSize = 0;

	// Stores a <name, offset> animation map
	std::map<std::string, gameAddr>* m_animNameToOffsetMap = nullptr;
	// Stores a <offset, offset> animation map
	std::map<gameAddr, gameAddr>* m_animOffsetToNameOffset = nullptr;

	// Returns false if the field's input buffer is invalid for the field type
	bool ValidateFieldType(EditorInput* field);
public:
	// Constants useful constant variables, to be set on a per-game basis
	std::map<EditorConstants_, unsigned int >* constants;
	// Contains quick data that is made to be accessed outside of this class
	EditorTable movesetTable;
	// Status of the animation extraction
	AnimExtractionStatus_ animationExtractionStatus = AnimExtractionStatus_NotStarted;
	// Thread where the animations extraction will be running to avoid blocking the display thread
	std::thread* animExtractionThread;
	// Stores the address of the loaded moveset in-game. Will become 0 if it does not match the current moveset.
	gameAddr live_loadedMoveset = 0;
	// If moveset posses movelist data
	bool hasDisplayableMovelist = false;
	// Contains every move, in a displayable format. Not tied to training mode movelist.
	std::vector<DisplayableMove*>* displayableMovelist;
	// Set to true when updating displayableMovelist
	bool mustReloadMovelist = false;

	Editor(GameProcess* process, GameData* game, uint16_t gameId, uint16_t minorVersion);
	virtual ~Editor();

	// Loads important moveset data in our class, required to start functionning
	virtual void LoadMoveset(Byte* t_moveset, uint64_t t_movesetSize) = 0;
	// Assigns the moveset pointer and useful ptrs to it
	virtual void LoadMovesetPtr(Byte* t_moveset, uint64_t t_movesetSize) = 0;
	// Return the movelist in a quickly displayable format
	virtual void ReloadDisplayableMoveList() = 0;
	// For the movelist displayed to the left
	virtual void RecomputeDisplayableMoveFlags(uint16_t moveId) = 0;
	// Returns the given player current move id
	virtual uint16_t GetCurrentMoveID(uint8_t playerId) = 0;
	// Returns the moveset and its size
	const Byte* GetMoveset(uint64_t& movesetSize_out);


	// Forms
	// Returns the fields to display to build a single form of a certain type (move, voiceclip, cancel extradata, etc...)
	virtual InputMap GetFormFields(EditorWindowType_ type, uint16_t id, VectorSet<std::string>& drawOrder) = 0;
	// Returns the fields to display to build a list of forms of a certain type (extraproperties, requirements, cancels, etc...)
	virtual std::vector<InputMap> GetFormFieldsList(EditorWindowType_ type, uint16_t id, VectorSet<std::string>& drawOrder) = 0;
	virtual std::vector<InputMap> GetFormFieldsList(EditorWindowType_ type, uint16_t id, VectorSet<std::string>& drawOrder, int listSize) = 0;
	virtual InputMap GetListSingleForm(EditorWindowType_ type, uint16_t id, VectorSet<std::string>& drawOrder) = 0;
	// Returns true if the given field is valid
	virtual bool ValidateField(EditorWindowType_ fieldType, EditorInput* field) = 0;
	// Save a single struct in the moveset
	virtual void SaveItem(EditorWindowType_ type, uint16_t id, InputMap& inputs) = 0;

	// -- Iteractions -- //
	// Sets the current move of a player
	virtual void SetCurrentMove(uint8_t playerId, gameAddr playerMoveset, size_t moveId) = 0;
	// Saves all the moveset animations in our library
	virtual void OrderAnimationsExtraction(const std::string& characterFilename) = 0;
	// Imports an animation into the moveset and applies it to a move. Returns the name of the imported anim.
	virtual std::string ImportAnimation(const wchar_t* filepath, int moveid) = 0;

	// -- Creation -- //
	// Create a new structure or structure list
	virtual uint32_t CreateNew(EditorWindowType_ type) = 0;
	// Modifies an existing list's size
	virtual void ModifyListSize(EditorWindowType_ type, unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds) = 0;

	// -- Command Utils -- //
	// Returns a command string from two seperate direction & button fields
	virtual std::string GetCommandStr(const char* direction, const char* button) = 0;
	// Returns a command string from a command field
	virtual std::string GetCommandStr(const char* commandBuf) = 0;
	// Writes the input sequence to string, or writes to outSize if the sequence is over MAX_INPUT_SEQUENCE_SHORT_LEN inputs
	virtual void GetInputSequenceString(int id, std::string& outStr, int& outSize) = 0;
	// Returns a command string 
	virtual std::string GetDisplayableMovelistInputStr(const char* directions, const char* buttons) = 0;
	// Returns the color of a MvlDisplayable field according to its type
	virtual int GetDisplayableMovelistEntryColor(EditorInput* field) = 0;
	// Returns true if a displayable entry in the movelist is a combo
	virtual bool IsMovelistDisplayableEntryCombo(EditorInput* field) = 0;
	// Returns true if a displayable entry in the movelist is a category (don't increment index)
	virtual	bool IsMovelistDisplayableEntryCategory(EditorInput* field) = 0;
	// -- Utils -- //
	// Returns true if the command is an input sequence
	virtual bool IsCommandInputSequence(const char* buffer) = 0;
	// Determines if a command states that the cancel refers to a grouped cancel 
	virtual bool IsCommandGroupedCancelReference(const char* buffer) = 0;
	// Returns the ID of an input sequence from a (grouped)cancel's command buffer
	virtual int GetCommandInputSequenceID(const char* buffer) = 0;
	// Returns true if the property ID refers to a throw camera
	virtual bool IsPropertyThrowCameraRef(const char* buffer) = 0;
	// Returns true if the property ID refers to a projectile
	virtual bool IsPropertyProjectileRef(const char* buffer) = 0;
	// Returns true if the voiceclip value indicates the end of the voiceclip list
	virtual bool IsVoicelipValueEnd(const char* buffer) = 0;
	// Returns the amount of structure in the given structure list type (move, cancel etc)
	virtual unsigned int GetStructureCount(EditorWindowType_ type) = 0;
	// Returns the amount of MOTA animations inside of a specific MOTA
	virtual unsigned int GetMotaAnimCount(int motaId) = 0;
	// Returns a string computed based on a movelist displayable's icons value and more
	virtual std::string GetMovelistDisplayableLabel(InputMap& fieldMap) = 0;
	// Returns the list of inputs in the displayable movelist
	virtual unsigned int GetMovelistDisplayableInputCount() = 0;

	// -- Live edition -- //
	// Called whenever a field is edited. Returns false if a re-import is needed.
	virtual void Live_OnFieldEdit(EditorWindowType_ type, int id, EditorInput* field) { };

	// CRC32 calculation
	virtual uint32_t CalculateCRC32() = 0;
};