#pragma once

#include <vector>

#include "Editor.hpp"
#include "Helpers.hpp"

#include "Structs_t7.h"

using namespace StructsT7;

namespace EditorT7Utils
{
	const extern std::map<unsigned char, std::string> mvlDisplay_characterConversion;

	// Converts editor displayable text ({1} -> {2}) to game text with 3 bytes icon characters
	std::string ConvertMovelistDisplayableTextToGameText(const char* str);
};

struct StructIterators
{
	StructIterator<StructsT7_gameAddr::Move> moves;
	StructIterator<Requirement> requirements;
	StructIterator<StructsT7_gameAddr::HitCondition> hit_conditions;
	StructIterator<StructsT7_gameAddr::Cancel> cancels;
	StructIterator<StructsT7_gameAddr::Cancel> grouped_cancels;
	StructIterator<StructsT7_gameAddr::Reactions> reactions;
	StructIterator<StructsT7_gameAddr::Pushback> pushbacks;
	StructIterator<PushbackExtradata> pushback_extras;
	StructIterator<CancelExtradata> cancel_extras;
	StructIterator<ExtraMoveProperty> extra_move_properties;
	StructIterator<StructsT7_gameAddr::OtherMoveProperty> move_start_properties;
	StructIterator<StructsT7_gameAddr::OtherMoveProperty> move_end_properties;
	StructIterator<StructsT7_gameAddr::Projectile> projectiles;
	StructIterator<StructsT7_gameAddr::InputSequence> input_sequences;
	StructIterator<StructsT7_gameAddr::ThrowCamera> throw_datas;
	StructIterator<Input> inputs;
	StructIterator<Voiceclip> voiceclips;
	StructIterator<CameraData> camera_datas;

	StructIterator<MvlDisplayable> mvl_displayables;
	StructIterator<MvlPlayable> mvl_playables;
	StructIterator<MvlInput> mvl_inputs;
};

class DLLCONTENT EditorT7 : public Editor
{
private:
	// Stores the moveset table containing lists offsets & count, aliases too
	MovesetInfo* m_infos = nullptr;
	// Contains iterators for the various structure lists
	StructIterators m_iterators;
	// Contains a ptr to the head of the displayable movelist
	MvlHead* m_mvlHead = nullptr;


	// Extra iterators setup
	void SetupIterators_DisplayableMovelist();

	// Moves
	InputMap GetMoveInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	uint64_t CreateMoveName(const char* newName);
	void SaveMoveName(const char* newName, gameAddr move_name_addr);
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
	std::vector<InputMap> GetMovelistDisplayablesInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveMovelistDisplayable(uint16_t id, InputMap& inputs);
	bool ValidateMovelistDisplayableField(EditorInput* field);

	//  Movelist : Playables
	InputMap GetMovelistPlayableInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveMovelistPlayable(uint16_t id, InputMap& inputs);
	bool ValidateMovelistPlayableField(EditorInput* field);

	// Movelist: Inputs
	std::vector<InputMap> GetMovelistInputListInputs(uint16_t id, int listSize, VectorSet<std::string>& drawOrder);
	void SaveMovelistInput(uint16_t id, InputMap& inputs);

	// -- Creation / Deletion -- //
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

	// -- List Creation / Deletion -- //
	void DisplayableMVLTranslationReallocate(int listId, int oldSize, int newSize, uint32_t listStart_offset);
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
	void ModifyMovelistDisplayableTextSize(int listId, int oldSize, int newSize);

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

	template<typename T> int ModifyGenericMovelistListSize(unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds, uint64_t listStart_offset)
	{
		// Compute list old and new sizes
		int newSize = (int)ids.size();
		int sizeDiff;
		int oldSize;
		{
			int newItemCount = 0;
			for (auto& id : ids)
			{
				if (id == -1) {
					newItemCount += 1;
				}
			}
			oldSize = ((int)ids.size() - newItemCount) + (int)deletedIds.size();
			sizeDiff = newSize - oldSize;
		}

		// Compute positions that will be used for copying
		int structSizeDiff = sizeof(T) * sizeDiff;
		uint64_t listPosition = listStart_offset + (sizeof(T) * listStart);
		uint64_t new_listEndPosition = listPosition + sizeof(T) * newSize;
		uint64_t old_listEndPosition = listPosition + sizeof(T) * oldSize;

		// Compute following m_header->offsets block position. We do this because we want to make sure it always stays 8 bytes aligned
		uint64_t new_followingBlockStart = m_movesetSize + structSizeDiff;
		uint64_t old_followingBlockStart = m_movesetSize;
		for (int i = 0; i < _countof(m_header->offsets.blocks); ++i)
		{
			uint64_t absoluteBlockAddr = (m_header->infos.moveset_data_start + m_header->offsets.blocks[i]);
			if (absoluteBlockAddr >= old_listEndPosition) {
				old_followingBlockStart = absoluteBlockAddr;
				new_followingBlockStart = Helpers::align8Bytes(old_followingBlockStart + structSizeDiff);
				break;
			}
		}

		// ------------------------------------------------
		// Compute new moveset size
		uint64_t newMovesetSize = new_followingBlockStart + (m_movesetSize - old_followingBlockStart);

		// Allocate new moveset
		Byte* newMoveset = (Byte*)calloc(1, newMovesetSize);
		if (newMoveset == nullptr) {
			return 0;
		}

		// ------------------------------------------------
		// Before we begin copying, there are a few values we want updated in the new moveset before it gets copied loaded up

		// Update count & table offsets right now so that iterators built from LoadMovesetPtr() are up to date
		{
			uint64_t movesetBlockStart = (m_header->infos.moveset_data_start + m_header->offsets.movesetBlock);
			uint64_t movesetBlockEnd = movesetBlockStart + (m_header->offsets.animationBlock - m_header->offsets.movesetBlock);

			if (movesetBlockStart < listPosition && listPosition < movesetBlockEnd)
			{
				unsigned int tableEntryCount = _countof(m_infos->table.entries);
				for (size_t i = 0; i < tableEntryCount; ++i)
				{
					auto& currentEntry = m_infos->table.entries[i];
					gameAddr absolute_entryStartAddr = movesetBlockStart + (uint64_t)currentEntry.listAddr;

					// listPosition be equal to a list's start both with and and without belonging to the list
					// Rule out the case where an empty structure list (count = 0) is resized into more
					// This is overkill and not likely to happen since ModifyListSize() is never called for list creations but modification of existing ones
					if (listPosition < absolute_entryStartAddr || \
						(listPosition == absolute_entryStartAddr && currentEntry.listCount != 0)) {
						*(uint64_t*)&currentEntry.listAddr += structSizeDiff;
						DEBUG_LOG("Shifting movesetblock entry %lld's offset\n", i);
					}
					else {
						bool isPartOfCurrentList = false;

						if (i + 1 == tableEntryCount) {
							isPartOfCurrentList = true;
						}
						else {
							auto& nextEntry = m_infos->table.entries[i + 1];
							gameAddr absolute_nextEntryStartAddr = movesetBlockStart + (uint64_t)nextEntry.listAddr;

							if (listPosition < absolute_nextEntryStartAddr || \
								(listPosition == absolute_nextEntryStartAddr && nextEntry.listCount != 0)) {
								isPartOfCurrentList = true;
							}
						}

						if (isPartOfCurrentList) {
							currentEntry.listCount += sizeDiff;
							DEBUG_LOG("Adding movelist entry %llu count += %d\n", i, sizeDiff);
						}
					}
				}
			}
		}

		// There are blocks to shift in the displayble movelist block
		{
			uint64_t movelistBlockStart = (m_header->infos.moveset_data_start + m_header->offsets.movelistBlock);
			uint64_t movelistBlockEnd = movelistBlockStart + m_mvlHead->inputs_offset + sizeof(MvlInput) * m_iterators.mvl_inputs.size();

			if (movelistBlockStart < listPosition && listPosition < movelistBlockEnd && strncmp(m_mvlHead->mvlString, "MVLT", 4) == 0)
			{
				uint64_t relativeOffset = listPosition - movelistBlockStart;

				if ((m_mvlHead->inputs_offset > relativeOffset) || (listStart != 0 && m_mvlHead->inputs_offset == relativeOffset)) {
					m_mvlHead->inputs_offset += structSizeDiff;
					DEBUG_LOG("MVLT: Shifted mvl_inputs block by %d (0x%x) bytes\n", structSizeDiff, structSizeDiff);

					if ((m_mvlHead->playables_offset > relativeOffset) || (listStart != 0 && m_mvlHead->playables_offset == relativeOffset)) {
						m_mvlHead->playables_offset += structSizeDiff;
						DEBUG_LOG("MVTL: Shifted mvl_playable block by %d (0x%x) bytes\n", structSizeDiff, structSizeDiff);

						if ((m_mvlHead->displayables_offset > relativeOffset) || (listStart != 0 && m_mvlHead->displayables_offset == relativeOffset)) {
							m_mvlHead->displayables_offset += structSizeDiff;
							DEBUG_LOG("MVLT: Shifted mvl_displayable block %d (0x%x) bytes\n", structSizeDiff, structSizeDiff);
						}
					}
				}
			}
		}

		// Global moveset blocks must be shifted, better here than later
		for (int i = 0; i < _countof(m_header->offsets.blocks); ++i)
		{
			if ((m_header->infos.moveset_data_start + m_header->offsets.blocks[i]) >= listPosition) {
				m_header->offsets.blocks[i] = Helpers::align8Bytes(m_header->offsets.blocks[i] + structSizeDiff);
				DEBUG_LOG("Shifted moveset block %d by 0x%x (before alignment)\n", i, structSizeDiff);
			}
		}

		// ------------------------------------------------
		// Copy up to list start
		memcpy(newMoveset, m_moveset, listPosition);

		// Copy list items, one by one
		// That way mere reordering + resize technically don't need SaveItem() to be re-applied
		{
			uint64_t writeOffset = listPosition - sizeof(T);
			uint64_t readOffset;
			auto it = deletedIds.begin();
			for (auto id : ids)
			{
				writeOffset += sizeof(T);

				if (id != -1) {
					// Copy old structure data from its previous position (likely to not have changed but this handles reodering just fine)
					readOffset = listPosition + (sizeof(T) * id);
				}
				else {
					// Try to use existing data from deleted lists. This can be useful in rare cases like MvlDisplayables.
					if (it != deletedIds.end()) {
						readOffset = listPosition + (sizeof(T) * id);
						std::advance(it, 1);
					}
					else {
						// Don't bother copying anything. Calloc() implies that every byte of this struct will be at 0 anyway, so rely on that and do not change it.
						continue;
					}
				}

				memcpy(newMoveset + writeOffset, m_moveset + readOffset, sizeof(T));
			}
		}

		// Copy everything after the list up to the next block
		memcpy(newMoveset + new_listEndPosition, m_moveset + old_listEndPosition, old_followingBlockStart - old_listEndPosition);

		// If there is a block afterward and not just the moveset end, copy everything from that block to the moveset end
		if (old_followingBlockStart != m_movesetSize) {
			memcpy(newMoveset + new_followingBlockStart, m_moveset + old_followingBlockStart, m_movesetSize - old_followingBlockStart);
		}

		// Free up old moveset from memory and assign the new one
		free(m_moveset);
		LoadMovesetPtr(newMoveset, newMovesetSize);

		return sizeDiff;
	};
public:
	using Editor::Editor;

	void LoadMovesetPtr(Byte* t_moveset, uint64_t t_movesetSize) override;
	void LoadMoveset(Byte* t_moveset, uint64_t t_movesetSize) override;
	void ReloadDisplayableMoveList() override;
	void RecomputeDisplayableMoveFlags(uint16_t moveId) override;
	uint16_t GetCurrentMoveID(uint8_t playerId) override;
	InputMap GetFormFields(EditorWindowType_ type, uint16_t id, VectorSet<std::string>& drawOrder) override;
	std::vector<InputMap> GetFormFieldsList(EditorWindowType_ type, uint16_t id, VectorSet<std::string>& drawOrder) override;
	std::vector<InputMap> GetFormFieldsList(EditorWindowType_ type, uint16_t id, VectorSet<std::string>& drawOrder, int listSize) override;
	InputMap GetListSingleForm(EditorWindowType_ type, uint16_t id, VectorSet<std::string>& drawOrder) override;
	bool ValidateField(EditorWindowType_ fieldType, EditorInput* field) override;
	void SaveItem(EditorWindowType_ type, uint16_t id, InputMap& inputs) override;

	// -- Interactions -- //
	// Sets the current move of a player
	void SetCurrentMove(uint8_t playerId, gameAddr playerMoveset, size_t moveId) override;
	// Starts a thread that extracts the moveset's animations
	void OrderAnimationsExtraction(const std::string& characterFilename) override;
	// Saves all the moveset animations in our library. Explicit copy of the parameters because this runs in another thread and the source data might be freed in parallel.
	void ExtractAnimations(Byte* moveset, std::string characterFilename, TKMovesetHeader_offsets offsets, std::map<gameAddr, uint64_t> animOffsetToNameOffset);
	// Imports an animation into the moveset and applies it to a move. Returns the name of the imported anim.
	std::string ImportAnimation(const char* filepath, int moveid) override;

	// -- Command Utils -- //
	std::string GetCommandStr(const char* direction, const char* button) override;
	std::string GetCommandStr(const char* commandBuf) override;
	void GetInputSequenceString(int id, std::string& outStr, int& outSize) override;
	std::string GetDisplayableMovelistInputStr(const char* directions, const char* buttons) override;
	int GetDisplayableMovelistEntryColor(EditorInput* field) override;
	bool IsMovelistDisplayableEntryCombo(EditorInput* field) override;
	bool IsMovelistDisplayableEntryCategory(EditorInput* field) override;
	// -- Utils --//
	bool IsCommandInputSequence(const char* buffer) override;
	bool IsCommandGroupedCancelReference(const char* buffer) override;
	int GetCommandInputSequenceID(const char* buffer) override;
	bool IsPropertyThrowCameraRef(const char* buffer) override;
	bool IsPropertyProjectileRef(const char* buffer) override;
	bool IsVoicelipValueEnd(const char* buffer) override;
	unsigned int GetStructureCount(EditorWindowType_ type) override;
	unsigned int GetMotaAnimCount(int motaId) override;
	// Movelist
	std::string GetMovelistDisplayableLabel(InputMap& fieldMap) override;
	unsigned int GetMovelistDisplayableInputCount() override;

	// -- Creation / Deletion -- //
	uint32_t CreateNew(EditorWindowType_ type) override;

	// -- List Creation / Deletion -- //
	void ModifyListSize(EditorWindowType_ type, unsigned int listStart, const std::vector<int>& ids, const std::set<int>& deletedIds) override;

	// -- Live edition -- //
	// Called whenever a field is edited. Returns false if a re-import is needed.
	void Live_OnFieldEdit(EditorWindowType_ type, int id, EditorInput* field) override;
};