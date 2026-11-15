#pragma once
#include "AnimationPreview/PreviewDisplacement.hpp"
#include "AnimationPreview/PreviewHitbox.hpp"
#include "RecordEdit.hpp"
#include <memory>

#include <vector>

#include "TEditor.hpp"
#include "Helpers.hpp"
#include "Online_t7.hpp"

#include "Structs_t7.h"

namespace EditorT7Utils
{
	const extern std::map<unsigned char, std::string> mvlDisplay_characterConversion;

	// Converts editor displayable text ({1} -> {2}) to game text with 3 bytes icon characters
	std::string ConvertMovelistDisplayableTextToGameText(const char* str);
};

class EditorT7 : public TEditor
{
private:
	// Contains a ptr to the offset list
	StructsT7::TKMovesetHeaderBlocks* m_offsets;
	// Stores a ptr to the moveset table containing lists offsets & count, aliases too
	StructsT7::MovesetInfo* m_infos = nullptr;
	// Contains a ptr to the head of the displayable movelist
	StructsT7::MvlHead* m_mvlHead = nullptr;
	// Contains iterators for the various structure lists
	struct
	{
		StructIterator<StructsT7_gameAddr::Move> moves;
		StructIterator<StructsT7::Requirement> requirements;
		StructIterator<StructsT7_gameAddr::HitCondition> hit_conditions;
		StructIterator<StructsT7_gameAddr::Cancel> cancels;
		StructIterator<StructsT7_gameAddr::Cancel> grouped_cancels;
		StructIterator<StructsT7_gameAddr::Reactions> reactions;
		StructIterator<StructsT7_gameAddr::Pushback> pushbacks;
		StructIterator<StructsT7::PushbackExtradata> pushback_extras;
		StructIterator<StructsT7::CancelExtradata> cancel_extras;
		StructIterator<StructsT7::ExtraMoveProperty> extra_move_properties;
		StructIterator<StructsT7_gameAddr::OtherMoveProperty> move_start_properties;
		StructIterator<StructsT7_gameAddr::OtherMoveProperty> move_end_properties;
		StructIterator<StructsT7_gameAddr::Projectile> projectiles;
		StructIterator<StructsT7_gameAddr::InputSequence> input_sequences;
		StructIterator<StructsT7_gameAddr::ThrowCamera> throw_datas;
		StructIterator<StructsT7::Input> inputs;
		StructIterator<StructsT7::Voiceclip> voiceclips;
		StructIterator<StructsT7::CameraData> camera_datas;

		StructIterator<StructsT7::MvlDisplayable> mvl_displayables;
		StructIterator<StructsT7::MvlPlayable> mvl_playables;
		StructIterator<StructsT7::MvlInput> mvl_inputs;
	} m_iterators;
	// Used to execute extra properties if handled by the game
	OnlineT7** m_sharedMemHandler = nullptr;


	// Extra iterators setup
	void SetupIterators_DisplayableMovelist();

	// Moves
    void RebuildAnimationMaps();
    bool NameRangeReferenced(uint64_t offset, uint64_t size) const;
	InputMap GetMoveInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	uint64_t CreateMoveName(const char* newName);
	uint64_t SaveMoveName(const char* newName, gameAddr move_name_addr);
	void SaveMove(uint16_t id, InputMap& inputs);
	bool ValidateMoveField(EditorInput* field);

	// Voiceclipss
	std::vector<InputMap> GetVoiceclipListInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveVoiceclip(uint16_t id, InputMap& inputs);

	// Extra properties
	std::vector<InputMap> GetExtrapropListInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveExtraproperty(uint16_t id, InputMap& inputs);

	// Other properties (start)
	std::vector<InputMap> GetMoveStartPropertyListInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveMoveStartProperty(uint16_t id, InputMap& inputs);
	bool ValidateOtherMoveProperty(EditorInput* field);

	// Other properties (end)
	std::vector<InputMap> GetMoveEndPropertyListInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveMoveEndProperty(uint16_t id, InputMap& inputs);

	// Cancels
	std::vector<InputMap> GetCancelListInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveCancel(uint16_t id, InputMap& inputs);
	bool ValidateCancelField(EditorInput* field);

	// Grouped Cancels
	std::vector<InputMap> GetGroupedCancelListInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveGroupedCancel(uint16_t id, InputMap& inputs);
	bool ValidateGroupedCancelField(EditorInput* field);

	// Cancel extras
	InputMap GetCancelExtraInput(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveCancelExtra(uint16_t id, InputMap& inputs);

	// Requirements
	std::vector<InputMap> GetRequirementListInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveRequirement(uint16_t id, InputMap& inputs);

	// Hit conditions
	std::vector<InputMap> GetHitConditionListInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveHitCondition(uint16_t id, InputMap& inputs);
	bool ValidateHitConditionField(EditorInput* field);

	// Reactions
	InputMap GetReactionsInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveReactions(uint16_t id, InputMap& inputs);
	bool ValidateReactionsField(EditorInput* field);

	// Pushback
	InputMap GetPushbackInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SavePushback(uint16_t id, InputMap& inputs);
	bool ValidatePushbackField(EditorInput* field);

	// Pushback extradata
	std::vector<InputMap> GetPushbackExtraListInputs(uint16_t id, int listSize, VectorSet<std::string>& drawOrder);
	void SavePushbackExtra(uint16_t id, InputMap& inputs);

	// Input Sequence
	InputMap GetInputSequenceInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveInputSequence(uint16_t id, InputMap& inputs);
	bool ValidateInputSequenceField(EditorInput* field);

	// Input list
	std::vector<InputMap> GetInputListInputs(uint16_t id, int listSize, VectorSet<std::string>& drawOrder);
	void SaveInput(uint16_t id, InputMap& inputs);

	// Projectiles
	InputMap GetProjectileInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveProjectile(uint16_t id, InputMap& inputs);
	bool ValidateProjectileField(EditorInput* field);

	// Throw Camera
	InputMap GetThrowCameraInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveThrowCamera(uint16_t id, InputMap& inputs);
	bool ValidateThrowCameraField(EditorInput* field);

	// Camera data
	InputMap GetCameraDataInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveCameraData(uint16_t id, InputMap& inputs);

	//  Movelist : Displayables
	std::vector<InputMap> GetMovelistDisplayablesInputs(uint16_t id, VectorSet<std::string>& drawOrder, bool singleItem=false);
	void SaveMovelistDisplayable(uint16_t id, InputMap& inputs);
	bool ValidateMovelistDisplayableField(EditorInput* field);

	//  Movelist : Playables
	InputMap GetMovelistPlayableInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveMovelistPlayable(uint16_t id, InputMap& inputs);
	bool ValidateMovelistPlayableField(EditorInput* field);

	// Movelist: Inputs
	std::vector<InputMap> GetMovelistInputListInputs(uint16_t id, int listSize, VectorSet<std::string>& drawOrder);
	void SaveMovelistInput(uint16_t id, InputMap& inputs);

	// -- Creation -- //
	// Create a new structure or structure list
	//
	uint32_t CreateInputSequence();
	uint32_t CreateInputList();
	//
	uint32_t CreateNewRequirements();
	//
	uint32_t CreateNewMove();
	uint32_t CreateNewVoiceclipList();
	uint32_t CreateNewCancelList();
	uint32_t CreateNewGroupedCancelList();
	uint32_t CreateNewCancelExtra();
	//
	uint32_t CreateNewExtraProperties();
	uint32_t CreateNewMoveBeginProperties();
	uint32_t CreateNewMoveEndProperties();
	//
	uint32_t CreateNewHitConditions();
	uint32_t CreateNewReactions();
	uint32_t CreateNewPushback();
	uint32_t CreateNewPushbackExtra();
	//
	uint32_t CreateNewProjectile();
	//
	uint32_t CreateNewThrowCamera();
	uint32_t CreateNewCameraData();
	//
	uint32_t CreateNewMvlPlayable();
	uint32_t CreateNewMvlInputs();
	// -- Single structure Deletion -- 
	void DeleteMove(int id);
	void DeleteCancelExtradata(int id);
	void DeleteReactions(int id);
	void DeletePushback(int id);
	void DeleteInputSequence(int id);
	void DeleteProjectile(int id);
	void DeleteThrowCamera(int id);
	void DeleteCameraData(int id);
	void DeleteMovelistPlayable(int id);

	// -- List Creation / Deletion -- //
	//
	void ModifyRequirementListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds);
	//
	void ModifyCancelListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds);
	void ModifyGroupedCancelListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds);
	//
	void ModifyExtraPropertyListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds);
	void ModifyStartPropertyListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds);
	void ModifyEndPropertyListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds);
	//
	void ModifyHitConditionListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds);
	void ModifyPushbackExtraListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds);
	//
	void ModifyInputListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds);
	//
	void ModifyVoiceclipListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds);
	// Movelist
	void ModifyMovelistInputSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds);
	void ModifyMovelistDisplayableSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds);

	// Duplication
	uint32_t DuplicateMove(uint32_t id, size_t listSize);
	uint32_t DuplicateCancelExtradata(uint32_t id, size_t listSize);
	uint32_t DuplicateReactions(uint32_t id, size_t listSize);
	uint32_t DuplicatePushback(uint32_t id, size_t listSize);
	uint32_t DuplicateInputSequence(uint32_t id, size_t listSize);
	uint32_t DuplicateProjectile(uint32_t id, size_t listSize);
	uint32_t DuplicateThrowCamera(uint32_t id, size_t listSize);
	uint32_t DuplicateCameraData(uint32_t id, size_t listSize);
	uint32_t DuplicateMovelistPlayable(uint32_t id, size_t listSize);

	uint32_t DuplicateRequirement(uint32_t id, size_t listSize);
	uint32_t DuplicateVoiceclip(uint32_t id, size_t listSize);
	uint32_t DuplicateCancel(uint32_t id, size_t listSize);
	uint32_t DuplicateGroupedCancel(uint32_t id, size_t listSize);
	uint32_t DuplicateExtraproperty(uint32_t id, size_t listSize);
	uint32_t DuplicateMoveBeginProperty(uint32_t id, size_t listSize);
	uint32_t DuplicateMoveEndProperty(uint32_t id, size_t listSize);
	uint32_t DuplicateHitCondition(uint32_t id, size_t listSize);
	uint32_t DuplicatePushbackExtradata(uint32_t id, size_t listSize);
	uint32_t DuplicateInput(uint32_t id, size_t listSize);
	uint32_t DuplicateMovelistInput(uint32_t id, size_t listSize);

	// -- Live edition -- //
	//
	void Live_OnMoveEdit(int id, EditorInput* field);
	void Live_OnVoiceclipEdit(int id, EditorInput* field);
	//
	void Live_OnCancelEdit(int id, EditorInput* field);
	void Live_OnGroupedCancelEdit(int id, EditorInput* field);
	void Live_OnCancelExtraEdit(int id, EditorInput* field);
	//
	void Live_OnExtrapropertyEdit(int id, EditorInput* field);
	void Live_OnMoveBeginPropEdit(int id, EditorInput* field);
	void Live_OnMoveEndPropEdit(int id, EditorInput* field);
	//
	void Live_OnRequirementEdit(int id, EditorInput* field);
	//
	void Live_OnHitConditionPropEdit(int id, EditorInput* field);
	void Live_OnPushbackEdit(int id, EditorInput* field);
	void Live_OnPushbackExtraEdit(int id, EditorInput* field);
	//
	void Live_OnInputSequenceEdit(int id, EditorInput* field);
	void Live_OnInputEdit(int id, EditorInput* field);
	//
	void Live_OnReactionsEdit(int id, EditorInput* field);
	//
	void Live_OnProjectileEdit(int id, EditorInput* field);
	//
	void Live_OnThrowCameraEdit(int id, EditorInput* field);
	void Live_OnCameraDataEdit(int id, EditorInput* field);
	//
	void Live_OnMovelistDisplayableEdit(int id, EditorInput* field);
	void Live_OnMovelistPlayableEdit(int id, EditorInput* field);
	void Live_OnMovelistInputEdit(int id, EditorInput* field);

	// Movelist text conversion
	std::string GetMovelistDisplayableText(uint32_t offset);

	// Templates
	template<typename T> int ModifyGenericMovelistListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds, uint64_t listStart_offset, RecordListCount count);

public:
	using TEditor::TEditor;

	void LoadMovesetPtr(Byte* t_moveset, uint64_t t_movesetSize) override;
	bool LoadMoveset(Byte* t_moveset, uint64_t t_movesetSize) override;
	void ReloadDisplayableMoveList() override;
	void RecomputeDisplayableMoveFlags(uint16_t moveId) override;
	uint16_t GetCurrentMoveID(uint8_t playerId) const override;
	InputMap GetFormFields(EditorWindowType type, uint16_t id, VectorSet<std::string>& drawOrder) override;
	std::vector<InputMap> GetFormFieldsList(EditorWindowType type, uint16_t id, VectorSet<std::string>& drawOrder) override;
	std::vector<InputMap> GetFormFieldsList(EditorWindowType type, uint16_t id, VectorSet<std::string>& drawOrder, int listSize) override;
	InputMap GetListSingleForm(EditorWindowType type, uint16_t id, VectorSet<std::string>& drawOrder) override;
	bool ValidateField(EditorWindowType fieldType, EditorInput* field) override;
	void SaveItem(EditorWindowType type, uint16_t id, InputMap& inputs) override;

	// -- Interactions -- //
	// Sets the current move of a player
	void SetCurrentMove(uint8_t playerId, gameAddr playerMoveset, size_t moveId) override;
	// Imports an animation into the moveset and applies it to a move. Returns the name of the imported anim.
	std::string ImportAnimation(const wchar_t* filepath, int moveid) override;
	// Copy of a move's body animation (T7 0x64 / 0xC8 bytes), for the animation preview. Throws when it cannot be read.
	std::vector<Byte> MoveAnimation(uint16_t moveId);
	// Changes when the move uses another animation (its offset in the animation block).
	uint64_t MoveAnimationToken(uint16_t moveId);
	// The move's extra properties that the animation preview applies (AnimPreview::T7DisplacementKind), as saved.
	std::vector<AnimPreview::Displacement> MoveDisplacements(uint16_t moveId);
	// Reaction moves of the move's hit on the animation preview's opponent: the first hit condition whose requirements
	// hold for an opponent airborne or not (only "opponent airborne / not airborne / in a juggle" are judged, other
	// requirements count as unmet), down to the list's default entry. Move ids resolved (aliases), -1 when none.
	AnimPreview::ReactionMoves MoveHitReactions(uint16_t moveId, bool opponentAirborne);
	// The move's hit uses the screw: one of its hit conditions requires 615 "Screw available".
	bool MoveUsesScrew(uint16_t moveId) const;
	// Move id with aliases (0x8000 + n) resolved; -1 when out of range.
	int32_t ResolveMoveId(uint32_t moveId) const;
	uint32_t MoveHitboxLocation(uint16_t moveId) { return moveId < m_iterators.moves.size() ? m_iterators.moves[moveId]->hitbox_location : 0; }
	uint32_t MoveCount() const { return static_cast<uint32_t>(m_iterators.moves.size()); }
	void DeleteAnimation(uint64_t anim_offset);
	void DeleteNameBlockString(uint64_t string_offset);
	void DeleteAnimationIfUnused(uint64_t anim_addr, uint64_t anim_name_addr);

	// -- Command Utils -- //
	std::string GetCommandStr(uint64_t command) const;
	std::string GetCommandStr(const char* direction, const char* button) const override;
	std::string GetCommandStr(const char* commandBuf) const override;
	void GetInputSequenceString(int id, std::string& outStr, int& outSize) const override;
	std::string GetDisplayableMovelistInputStr(const char* directions, const char* buttons) const override;
	int GetDisplayableMovelistEntryColor(EditorInput* field) const override;
	bool IsMovelistDisplayableEntryCombo(EditorInput* field) const override;
	bool IsMovelistDisplayableEntryCategory(EditorInput* field) const override;
	// -- Utils --//
	bool IsCommandInputSequence(const char* buffer) const override;
	bool IsCommandInputSequence(uint64_t command) const;
	bool IsCommandGroupedCancelReference(uint64_t command) const;
	bool IsCommandGroupedCancelReference(const char* buffer) const override;
	int GetCommandInputSequenceID(const char* buffer) const override;
	bool IsPropertyThrowCameraRef(const char* buffer) const override;
	bool IsPropertyThrowCameraRef(uint32_t id) const;
	bool IsPropertyProjectileRef(const char* buffer) const override;
	bool IsPropertyProjectileRef(uint32_t id) const;
	bool IsVoicelipValueEnd(const char* buffer) const override;
	unsigned int GetStructureCount(EditorWindowType type) const override;
	unsigned int GetMotaAnimCount(int motaId) const override;
	// Movelist
	std::string GetMovelistDisplayableLabel(InputMap& fieldMap) const override;
	unsigned int GetMovelistDisplayableInputCount() const override;
	//
	const char* GetMoveName(uint32_t moveid) const override;

	// -- Creation  -- //
	uint32_t CreateNew(EditorWindowType type) override;

	// -- Copying / Deletion -- //
	uint32_t DuplicateStructure(EditorWindowType type, uint32_t id, size_t listSize) override;
	void DeleteStructures(EditorWindowType type, uint32_t id, size_t listSize) override;

	// -- List Creation / List Deletion -- //
	void ModifyListSize(EditorWindowType type, unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds) override;

	// -- Live edition -- //
	// Called whenever a field is edited. Returns false if a re-import is needed.
	void Live_OnFieldEdit(EditorWindowType type, int id, EditorInput* field) override;

	// CRC32 calculation
	uint32_t CalculateCRC32() override;

	// Shared memory, used for extra properties execution
	void SetSharedMemHandler(Online** sharedMemHandler) override;
	void ExecuteExtraprop(EditorInput* idField, EditorInput* valueField) override;

	// References
	std::vector<TEditor::CancelProjectileReference> ListCancelsProjectilesReferences(unsigned int cancel_start_id) const override;
	std::vector<TEditor::CancelMoveReference> ListCancelsMoveReferences(unsigned int cancel_start_id) const override;
	std::vector<MoveCancelReference> ListMoveCancelsReferences(unsigned int moveid) const override;
	std::vector<MoveGroupedCancelReference> ListMoveGroupedCancelsReferences(unsigned int moveid) const override;
	std::vector<MoveReactionsReference> ListMoveReactionsReferences(unsigned int moveid) const override;
};

template<typename T> int EditorT7::ModifyGenericMovelistListSize(unsigned int listStart,
    const std::vector<int>& ids, const std::set<int>& deletedIds, uint64_t listHead,
    RecordListCount count)
{
    const auto [listCount, countField, countWidth] = count;
    const auto dataStart = m_header->moveset_data_start;
    const bool movelist = countWidth == 0 || countField >= dataStart + m_offsets->movelistBlock;
    std::vector<size_t> structureFields;
    std::vector<size_t> movelistFields;
    if (!movelist) {
        for (auto& entry : m_infos->table.entries)
            structureFields.push_back(reinterpret_cast<Byte*>(&entry.listAddr) - m_moveset);

    } else {
        movelistFields = {
            static_cast<size_t>(reinterpret_cast<Byte*>(&m_mvlHead->displayables_offset) - m_moveset),
            static_cast<size_t>(reinterpret_cast<Byte*>(&m_mvlHead->playables_offset) - m_moveset),
            static_cast<size_t>(reinterpret_cast<Byte*>(&m_mvlHead->inputs_offset) - m_moveset)};
    }
    size_t targetMovelistField = 0;
    if (movelist) {
        const auto displayableCount = reinterpret_cast<Byte*>(&m_mvlHead->displayables_count) - m_moveset;
        targetMovelistField = movelistFields[countWidth == 0 ? 2 : countField == displayableCount ? 0 : 1];
    }
    auto result = BuildRecordEdit({m_moveset, static_cast<size_t>(m_movesetSize)}, sizeof(T), listHead,
        listCount, countField, countWidth, listStart, ids, deletedIds,
        m_header->block_list, m_header->block_list_size, dataStart,
        movelist ? StructsT7::TKMovesetHeaderBlocks_Movelist : StructsT7::TKMovesetHeaderBlocks_Moveset,
        structureFields, dataStart + (movelist ? m_offsets->movelistBlock : m_offsets->movesetBlock), movelistFields, targetMovelistField);
    if (movelist) {
        RecordEdit edit(listStart, ids, deletedIds, listCount);
        const auto oldEnd = listHead + (listStart + edit.oldCount) * sizeof(T) - dataStart - m_offsets->movelistBlock;
        const int64_t delta = static_cast<int64_t>(edit.difference) * sizeof(T);
        auto mvl = reinterpret_cast<StructsT7::MvlHead*>(result.bytes.get() + dataStart + m_offsets->movelistBlock);
        auto rows = reinterpret_cast<StructsT7::MvlDisplayable*>(reinterpret_cast<Byte*>(mvl) + mvl->displayables_offset);
        for (int i = 0; i < mvl->displayables_count; ++i)
            for (auto& offset : rows[i].all_translation_offsets)
                if (offset >= 0 && static_cast<uint64_t>(offset) >= oldEnd) offset += static_cast<int32_t>(delta);
    }
    free(m_moveset);
    LoadMovesetPtr(result.bytes.release(), result.size);
    return result.difference;
}
