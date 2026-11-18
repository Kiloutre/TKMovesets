#pragma once
#include <type_traits>
#include <stdexcept>
#include <memory>
#include <span>
#include <string_view>
#include <optional>
#include <cstring>
#include <stdexcept>
#include "RecordEdit.hpp"

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

#include "constants.h"
#include "GameTypes.h"
#include "MovesetStructs.h"

# define FORM_BUFSIZE (32) // Regular fields
# define FORM_STRING_BUFSIZE (128) // String fields
# define FORM_INPUT_MAX_BUFSIZE (256) // Some very long fields like displayable movelist use this
# define MAX_INPUT_SEQUENCE_SHORT_LEN (15)

// Shifting logic for list resizing
# define MUST_SHIFT_ID(id, valueChange, listStart, listOldEnd) EditorUtils::MustShiftId(id, valueChange, listStart, listOldEnd)

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
	EditorConstants_InputSequenceCommandStart,
	// Marks a projectile in the extraprop list
	EditorConstants_ProjectileProperty,
};

typedef unsigned int EditorWindowType;
enum EditorWindowType_
{
	// Invalid must always be 0
	EditorWindowType_INVALID = 0
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
	// Description shown on hover
	std::string hoverDescription;
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
	// Text of the last applied value, for changed-field marking (set by editors that use it, unset otherwise)
	std::optional<std::string> savedText;
	// Used to implement pasting from clipboard. Can't write into a focused field.
	std::optional<std::string> nextValue;
    void QueueValue(std::string value) {
        if (!buffer || !bufsize || value.size() >= bufsize || value.find('\0') != value.npos)
            throw std::length_error("Pasted text does not fit this field.");
        nextValue = std::move(value);
    }
    bool ConsumeNextValue() {
        if (!nextValue) return false;
        if (!buffer || !bufsize || nextValue->size() >= bufsize || nextValue->find('\0') != nextValue->npos)
            throw std::length_error("Pending text does not fit this field.");
        memcpy(buffer, nextValue->c_str(), nextValue->size() + 1);
        nextValue.reset();
        return true;
    }
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
	// The move's hit uses the screw (tornado): a hit condition requires "screw available". Shown as an icon.
	bool screw = false;
	// The move spends rage: one of its properties is "Spend Rage" with parameter 1. Shown as an icon.
	bool rage = false;
};

struct DeleteEditorInput {
    void operator()(EditorInput* field) const noexcept {
        if (field) { delete[] field->buffer; delete field; }
    }
};
using OwnedEditorInput = std::unique_ptr<EditorInput, DeleteEditorInput>;

// Own fields and buffers; category/draw-order maps only borrow their pointers.
// Copying would duplicate ownership, so factory returns and row transfers must move.
class InputMap : private std::map<FasterStringComp, EditorInput*> {
    using Base = std::map<FasterStringComp, EditorInput*>;
public:
    using Base::size; using Base::empty; using Base::contains;
    auto begin() const noexcept { return Base::begin(); }
    auto end() const noexcept { return Base::end(); }
    auto find(const FasterStringComp& name) const { return Base::find(name); }
    EditorInput* const& at(const FasterStringComp& name) const { return Base::at(name); }
    EditorInput* const& operator[](const FasterStringComp& name) const { return Base::at(name); }
    InputMap() = default;
    InputMap(const InputMap&) = delete;
    InputMap& operator=(const InputMap&) = delete;
    InputMap(InputMap&& other) noexcept { Base::swap(other); }
    InputMap& operator=(InputMap&& other) noexcept {
        if (this != &other) { clear(); Base::swap(other); }
        return *this;
    }
    ~InputMap() { clear(); }
    void clear() noexcept {
        for (auto& [name, field] : *this) DeleteEditorInput{}(field);
        Base::clear();
    }
    void swap(InputMap& other) noexcept { Base::swap(other); }
    EditorInput* Own(const std::string& name, OwnedEditorInput field) {
        if (!field) throw std::invalid_argument("Cannot own a null editor field.");
        if (!Base::emplace(name, field.get()).second)
            throw std::invalid_argument("Duplicate editor field: " + name);
        return field.release();
    }
};

namespace EditorUtils
{
	// Define structure ID shifting logic
	bool MustShiftId(gameAddr32 structureId, int valueChange, int listStart, int old_listEnd);
	bool MustShiftId(gameAddr structureId, int valueChange, int listStart, int old_listEnd);
	bool MustShiftId(int structureId, int valueChange, int listStart, int old_listEnd);
	// Used for color coding input fields
	void SetInputfieldColor(EditorInput* field);
	// Returns a format depending on the input type
	// Defined here: constexpr functions are inline, and the templates below use it from other files.
	constexpr const char* GetFieldFormat(EditorInputFlag flags)
	{
		if (flags & (EditorInput_H64)) {
			return "0x%llX";
		}
		if (flags & (EditorInput_H32 | EditorInput_H16 | EditorInput_H8)) {
			return "0x%X";
		}
		if (flags & EditorInput_S64) {
			return "%lld";
		}
		if (flags & EditorInput_U64) {
			return "%llu";
		}
		if (flags & (EditorInput_U32 | EditorInput_U16 | EditorInput_U8)) {
			return "%u";
		}
		if (flags & (EditorInput_Float)) {
			return "%.7g";
		}
		if (flags & (EditorInput_String)) {
			return "%s";
		}
		return "%d";
	}
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
		if (inputMap.contains(fieldName)) throw std::invalid_argument("Duplicate editor field: " + fieldName);

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

		auto buffer = std::make_unique<char[]>(bufsize);
		OwnedEditorInput newField(new EditorInput{
			.name = fieldName,
			.category = category,
			.imguiInputFlags = EditorUtils::GetFieldCharset(flags),
			.flags = flags,
			.buffer = buffer.get(),
			.bufsize = bufsize,
        });
        buffer.release();
        EditorUtils::SetInputfieldColor(newField.get());
        if (_snprintf_s(newField->buffer, newField->bufsize, _TRUNCATE, EditorUtils::GetFieldFormat(flags), value) < 0)
            throw std::length_error("Initial editor value exceeds field capacity.");
        const auto oldOrderSize = drawOrder.size();
        drawOrder.push_back(fieldName);
        try { return inputMap.Own(fieldName, std::move(newField)); }
        catch (...) { drawOrder.resize(oldOrderSize); throw; }
	}
}

class EditorLogic : public BaseGameSpecificClass
{
public:
    class Mutation;
protected:
    Mutation* m_activeBatch = nullptr;
	// Contains our header data
	TKMovesetHeader* m_header = nullptr;

	// Contains the moveset, including our own header.
	Byte* m_moveset = nullptr;
	// Contains the size of the moveset, including our own header
	uint64_t m_movesetSize = 0;
	// Contains the moveset, without our header
	Byte* m_movesetData = nullptr;
	// Contains the moveset size, without our header
	uint64_t m_movesetDataSize = 0;

	template<class Count> RecordListCount DescribeRecordCount(const Count& count) const {
        return {static_cast<uint64_t>(count),
            static_cast<uint64_t>(reinterpret_cast<const Byte*>(&count) - m_moveset), sizeof(count)};
    }
    struct MutationState {
        virtual ~MutationState() = default;
        virtual void Restore() noexcept = 0;
        virtual void Commit() noexcept {}
    };
    virtual std::unique_ptr<MutationState> CaptureMutationState() { return {}; }
    // Replaces bytes inside one payload section; publishes only a complete, bounded allocation.
    void ReplaceBlockRange(size_t block, size_t offset, size_t removed, std::span<const Byte> inserted);
    std::string_view BlockString(size_t block, uint64_t offset) const;
    static std::string_view InputString(const EditorInput* input);
    virtual void RestoreMovesetPtr(Byte* data, uint64_t size) { LoadMovesetPtr(data,size); }
    virtual Byte* AllocateMutationBuffer(size_t size) { return static_cast<Byte*>(malloc(size)); }

	// Returns false if the field's input buffer is invalid for the field type
	bool ValidateFieldType(EditorInput* field);
public:
    // A transaction owns the previous allocation until the complete operation succeeds.
    class Mutation
    {
        EditorLogic& m_editor;
        Byte* m_original;
        uint64_t m_size;
        gameAddr m_liveAddress;
        std::unique_ptr<MutationState> m_state;
        Mutation* m_parentBatch;
        bool m_registered = false;
        bool m_failed = false;
        friend class EditorLogic;
    public:
        explicit Mutation(EditorLogic& editor, bool batch = false);
        ~Mutation();
        void Commit();
        Mutation(const Mutation&) = delete;
        Mutation& operator=(const Mutation&) = delete;
    };

    template<class Operation>
    auto Mutate(Operation&& operation)
    {
        if (m_activeBatch) {
            if (m_activeBatch->m_failed) throw std::runtime_error("Edit batch has already failed");
            try {
                return operation();
            } catch (...) {
                m_activeBatch->m_failed = true;
                throw;
            }
        }
        Mutation transaction(*this);
        if constexpr (std::is_void_v<std::invoke_result_t<Operation>>) {
            operation();
            transaction.Commit();
        } else {
            auto result = operation();
            transaction.Commit();
            return result;
        }
    }

    // Explicit batches are all-or-nothing. A caught child failure still prevents Commit.
    // Construct Mutation directly for an independent nested savepoint.
    template<class Operation>
    auto MutateBatch(Operation&& operation)
    {
        if (m_activeBatch) return Mutate(std::forward<Operation>(operation));
        Mutation batch(*this, true);
        if constexpr (std::is_void_v<std::invoke_result_t<Operation>>) {
            operation();
            batch.Commit();
        } else {
            auto result = operation();
            batch.Commit();
            return result;
        }
    }

	// Constants useful constant variables, to be set on a per-game basis
	std::map<EditorConstants_, unsigned int > constants;
	// Stores the address of the loaded moveset in-game. Will become 0 if it does not match the current moveset.
	gameAddr live_loadedMoveset = 0;
	// Id of the currently selected player, used for extra properties execution
	uint32_t currentPlayerId = 0;

	EditorLogic(GameProcess& process, GameData& game, const GameInfo* gameInfo);
	virtual ~EditorLogic();

	// Loads important moveset data in our class, required to start functionning. Returns false on error.
	virtual bool LoadMoveset(Byte* t_moveset, uint64_t t_movesetSize) = 0;
	// Assigns the moveset pointer and useful ptrs to it
	virtual void LoadMovesetPtr(Byte* t_moveset, uint64_t t_movesetSize) = 0;
	// Returns the moveset and its size
	const Byte* GetMoveset(uint64_t& movesetSize_out);


    std::vector<Byte> BuildSaveSnapshot();

	// Forms
	// Returns the fields to display to build a single form of a certain type (move, voiceclip, cancel extradata, etc...)
	virtual InputMap GetFormFields(EditorWindowType type, uint16_t id, VectorSet<std::string>& drawOrder) = 0;
	// Returns the fields to display to build a list of forms of a certain type (extraproperties, requirements, cancels, etc...)
	virtual std::vector<InputMap> GetFormFieldsList(EditorWindowType type, uint16_t id, VectorSet<std::string>& drawOrder) = 0;
	virtual std::vector<InputMap> GetFormFieldsList(EditorWindowType type, uint16_t id, VectorSet<std::string>& drawOrder, int listSize) = 0;
	virtual InputMap GetListSingleForm(EditorWindowType type, uint16_t id, VectorSet<std::string>& drawOrder) = 0;
	// Returns true if the given field is valid
	virtual bool ValidateField(EditorWindowType fieldType, EditorInput* field) = 0;
	// Save a single struct in the moveset
	virtual void SaveItem(EditorWindowType type, uint16_t id, InputMap& inputs) = 0;

	// -- Iteractions -- //
	// Imports an animation into the moveset and applies it to a move. Returns the name of the imported anim.
    // Wide row IDs are required by retail tables; legacy editors keep their u16 bounds.
    virtual InputMap GetListSingleFormWide(EditorWindowType type,uint32_t id,VectorSet<std::string>& order) {
        if(id>UINT16_MAX)throw std::out_of_range("Legacy record ID exceeds 16 bits");return GetListSingleForm(type,uint16_t(id),order);
    }
    virtual std::vector<InputMap> GetFormFieldsListWide(EditorWindowType type,uint32_t id,VectorSet<std::string>& order,int count) {
        if(id>UINT16_MAX)throw std::out_of_range("Legacy list ID exceeds 16 bits");return count?GetFormFieldsList(type,uint16_t(id),order,count):GetFormFieldsList(type,uint16_t(id),order);
    }
    virtual void SaveItemWide(EditorWindowType type,uint32_t id,InputMap& fields) {
        if(id>UINT16_MAX)throw std::out_of_range("Legacy record ID exceeds 16 bits");SaveItem(type,uint16_t(id),fields);
    }
	virtual std::string ImportAnimation(const wchar_t* filepath, int moveid) = 0;

	// -- Creation -- //
	// Create a new structure or structure list
	virtual uint32_t CreateNew(EditorWindowType type) = 0;
	// Modifies an existing list's size
	virtual void ModifyListSize(EditorWindowType type, unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds) = 0;

	// -- Copying / Deletion -- //
	virtual uint32_t DuplicateStructure(EditorWindowType type, uint32_t id, size_t listSize) = 0;
	virtual void DeleteStructures(EditorWindowType type, uint32_t id, size_t listSize) = 0;

	// -- Live edition -- //
	// Called whenever a field is edited. Returns false if a re-import is needed.
	virtual void Live_OnFieldEdit(EditorWindowType type, int id, EditorInput* field) { };
	// Called after a form or list was applied while live edition was bound to `live` (the in-game moveset). True when
	// the editor brought it up to date with the applied bytes, so live edition stays bound; false ends it (a new
	// import is needed).
	virtual bool Live_OnApplied(gameAddr live) { return false; }

	// CRC32 calculation
	virtual uint32_t CalculateCRC32() = 0;

	// Shared memory, used for extra properties execution
	virtual void SetSharedMemHandler(Online** sharedMemHandler) {}
};
