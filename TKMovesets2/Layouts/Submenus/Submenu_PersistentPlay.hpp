#pragma once

#include "GameSharedMem.hpp"

class Submenu_PersistentPlay {
private:
	// Orders the loading of a moveset in the game memory and set it as the local player's moveset
	void SelectMoveset(movesetInfo* moveset);
public:
	// Helper that contains both an importer and a shared memory manager
	GameSharedMem* gameHelper;

	void Render();
	// Renders the movelist
	void RenderMovesetList(bool canSelectMoveset);
};