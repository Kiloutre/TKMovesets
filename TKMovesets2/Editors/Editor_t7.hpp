#pragma once

#include <vector>

#include "Editor.hpp"

#include "Structs_t7.h"

using namespace StructsT7;

class EditorT7 : public Editor
{
private:
	MovesetInfo* m_infos = nullptr;

public:
	// Inherit constructor
	using Editor::Editor;

	void LoadMoveset(Byte* t_moveset, uint64_t t_movesetSize) override;
	EditorTable GetMovesetTable() override;
	std::vector<DisplayableMove*> GetDisplayableMoveList() override;
	uint16_t GetCurrentMoveID(uint8_t playerId) override;
};