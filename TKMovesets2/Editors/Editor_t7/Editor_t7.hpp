#pragma once

#include <vector>

#include "Editor.hpp"

#include "Structs_t7.h"

using namespace StructsT7;

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
};

class DLLCONTENT EditorT7 : public Editor
{
private:
	// Stores the moveset table containing lists offsets & count, aliases too
	MovesetInfo* m_infos = nullptr;
	// Stores absolute pointers to the moveset table within the moveset
	MovesetTable* m_absoluteTable = nullptr;
	std::vector<uint16_t> m_aliases;
	StructIterators m_iterators;

public:
	// Inherit constructor
	using Editor::Editor;

	void LoadMovesetPtr(Byte* t_moveset, uint64_t t_movesetSize) override;
	void LoadMoveset(Byte* t_moveset, uint64_t t_movesetSize) override;
	EditorTable GetMovesetTable() override;
	void ReloadDisplayableMoveList(std::vector<DisplayableMove*>* ref = nullptr) override;
	uint16_t GetCurrentMoveID(uint8_t playerId) override;
	std::map<std::string, EditorInput*> GetFormFields(EditorWindowType_ type, uint16_t id, VectorSet<std::string>& drawOrder) override;
	std::vector<std::map<std::string, EditorInput*>> GetFormFieldsList(EditorWindowType_ type, uint16_t id, VectorSet<std::string>& drawOrder) override;
	bool ValidateField(EditorWindowType_ fieldType, std::string fieldShortName, EditorInput* field) override;
	void SaveItem(EditorWindowType_ type, uint16_t id, std::map<std::string, EditorInput*>& inputs) override;
	
	// Moves
	std::map<std::string, EditorInput*> GetMoveInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	uint64_t CreateMoveName(const char* newName);
	void SaveMoveName(const char* newName, gameAddr move_name_addr);
	void SaveMove(uint16_t id, std::map<std::string, EditorInput*>& inputs);
	bool ValidateMoveField(std::string name, EditorInput* field);

	// Voiceclips
	std::map<std::string, EditorInput*> GetVoiceclipInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveVoiceclip(uint16_t id, std::map<std::string, EditorInput*>& inputs);

	// Extra properties
	std::vector<std::map<std::string, EditorInput*>> GetExtrapropListInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveExtraproperty(uint16_t id, std::map<std::string, EditorInput*>& inputs);

	// Other properties (start)
	std::vector<std::map<std::string, EditorInput*>> GetMoveStartPropertyListInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveMoveStartProperty(uint16_t id, std::map<std::string, EditorInput*>& inputs);
	bool ValidateOtherMoveProperty(std::string name, EditorInput* field);

	// Other properties (end)
	std::vector<std::map<std::string, EditorInput*>> GetMoveEndPropertyListInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveMoveEndProperty(uint16_t id, std::map<std::string, EditorInput*>& inputs);

	// Cancels
	std::vector<std::map<std::string, EditorInput*>> GetCancelListInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveCancel(uint16_t id, std::map<std::string, EditorInput*>& inputs);
	bool ValidateCancelField(std::string name, EditorInput* field);

	// Grouped Cancels
	std::vector<std::map<std::string, EditorInput*>> GetGroupedCancelListInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveGroupedCancel(uint16_t id, std::map<std::string, EditorInput*>& inputs);
	bool ValidateGroupedCancelField(std::string name, EditorInput* field);

	// Cancel extras
	std::map<std::string, EditorInput*> GetCancelExtraInput(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveCancelExtra(uint16_t id, std::map<std::string, EditorInput*>& inputs);

	// Requirements
	std::vector<std::map<std::string, EditorInput*>> GetRequirementListInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveRequirement(uint16_t id, std::map<std::string, EditorInput*>& inputs);

	// Hit conditions
	std::vector<std::map<std::string, EditorInput*>> GetHitConditionListInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveHitCondition(uint16_t id, std::map<std::string, EditorInput*>& inputs);
	bool ValidateHitConditionField(std::string name, EditorInput* field);

	// Reactions
	std::map<std::string, EditorInput*> GetReactionsInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SaveReactions(uint16_t id, std::map<std::string, EditorInput*>& inputs);
	bool ValidateReactionsField(std::string name, EditorInput* field);

	// Pushback
	std::map<std::string, EditorInput*> GetPushbackInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SavePushback(uint16_t id, std::map<std::string, EditorInput*>& inputs);
	bool ValidatePushbackField(std::string name, EditorInput* field);

	// Pushback extradata
	std::map<std::string, EditorInput*> GetPushbackExtraInputs(uint16_t id, VectorSet<std::string>& drawOrder);
	void SavePushbackExtra(uint16_t id, std::map<std::string, EditorInput*>& inputs);
	
	// -- Interactions -- //
	// Sets the current move of a player
	void SetCurrentMove(uint8_t playerId, gameAddr playerMoveset, size_t moveId) override;

	// -- Creation / Deletion -- //
	// Create a new structure or structure list
	int32_t CreateNew(EditorWindowType_ type) override;
	template<typename T> int32_t CreateNewGeneric(T* struct_1, T* struct_2, size_t tableListOffset);
	//
	int32_t CreateNewRequirements();
	//
	int32_t CreateNewMove();
	int32_t CreateNewVoiceclip();
	int32_t CreateNewCancelList();
	int32_t CreateNewGroupedCancelList();
	int32_t CreateNewCancelExtra();
	//
	int32_t CreateNewExtraProperties();
	int32_t CreateNewMoveBeginProperties();
	int32_t CreateNewMoveEndProperties();
	//
	int32_t CreateNewHitConditions();
	int32_t CreateNewReactions();
	int32_t CreateNewPushback();
	int32_t CreateNewPushbackExtra();

	// -- List Creation / Deletion -- //
	void ModifyListSize(EditorWindowType_ type, int listId, int oldSize, int newSize) override;
	template<typename T> void ModifyGenericListSize(int listId, int oldSize, int newSize, size_t tableListOffset);
	//
	void ModifyRequirementListSize(int listId, int oldSize, int newSize);
	//
	void ModifyCancelListSize(int listId, int oldSize, int newSize);
	void ModifyGroupedCancelListSize(int listId, int oldSize, int newSize);
	//
	void ModifyExtraPropertyListSize(int listId, int oldSize, int newSize);
	void ModifyStartPropertyListSize(int listId, int oldSize, int newSize);
	void ModifyEndPropertyListSize(int listId, int oldSize, int newSize);
	//
	void ModifyHitConditionListSize(int listId, int oldSize, int newSize);

};
