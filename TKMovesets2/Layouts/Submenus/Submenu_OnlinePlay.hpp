#pragma once

#include "GameSharedMem.hpp"

class Submenu_OnlinePlay {
private:

public:
	void Render();

	// Helper that contains both an importer and a shared memory manager
	GameSharedMem* gameHelper;
};