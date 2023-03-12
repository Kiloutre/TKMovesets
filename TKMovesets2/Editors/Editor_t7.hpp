#pragma once

#include <vector>

#include "Editor.hpp"

#include "Structs_t7.h"

using namespace StructsT7;

class DLLCONTENT EditorT7 : public Editor
{
private:
	// Stores the moveset table containing lists offsets & count, aliases too
	MovesetInfo* m_infos = nullptr;
	std::vector<uint16_t> m_aliases;

public:
	// Inherit constructor
	using Editor::Editor;

	void LoadMoveset(Byte* t_moveset, uint64_t t_movesetSize) override;
	EditorTable GetMovesetTable() override;
	std::vector<DisplayableMove*> GetDisplayableMoveList() override;
	uint16_t GetCurrentMoveID(uint8_t playerId) override;
	bool ValidateField(std::string fieldType, std::string fieldShortName, EditorInput* field) override;
	
	// Moves
	std::map<std::string, EditorInput*> GetMoveInputs(uint16_t moveId, std::vector<std::string>& drawOrder) override;
	void SaveMove(uint16_t moveId, std::map<std::string, EditorInput*>& inputs) override;
	bool ValidateMoveField(std::string name, EditorInput* field);

	// -- Iteractons -- //
	// Sets the current move of a player
	void SetCurrentMove(uint8_t playerId, gameAddr playerMoveset, size_t moveId) override;
};
