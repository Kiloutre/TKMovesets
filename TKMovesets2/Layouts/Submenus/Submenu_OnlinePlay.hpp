#pragma once

#include "GameImport.hpp"
#include "Online.hpp"

class Submenu_OnlinePlay {
private:
	bool m_isAttached = false;
	Online* m_onlineHandler = nullptr;

	// Called once when a process is attached
	void OnAttach();
	// Called once when a process is detached
	void OnDetach();
public:
	~Submenu_OnlinePlay();
	void Render();

	void SetTargetProcess(const char* processName, uint8_t gameId);
	// Store our own copy of the importer to not interfere with the other one. Not important but less prone to problems, really.
	GameImport gameHelper;
};