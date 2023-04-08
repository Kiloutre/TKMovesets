#pragma once

#include "GameImport.hpp"

class Submenu_OnlinePlay {
public:
	void Render();

	void SetTargetProcess(const char* processName, uint8_t gameId);
	// Store our own copy of the importer to not interfere with the other one. Not important but less prone to problems, really.
	GameImport importerHelper;
};