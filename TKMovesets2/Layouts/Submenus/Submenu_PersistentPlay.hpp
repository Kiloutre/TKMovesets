#pragma once

#include "GameSharedMem.hpp"

class Submenu_PersistentPlay {
private:
	int32_t m_currentPlayerCursor = -1;

	// Orders the loading of a moveset in the game memory and set it as the local player's moveset
	void SelectMoveset(movesetInfo* moveset);
	// Orders that a moveset is to be de-associated from a player
	void ClearMoveset();

public:
	// Helper that contains both an importer and a shared memory manager
	GameSharedMem* gameHelper;

	void Render();
	// Renders the movelist
	void RenderMovesetList(bool canSelectMoveset);
	// Render the game selector, dll inject button
	void RenderGameControls();
};