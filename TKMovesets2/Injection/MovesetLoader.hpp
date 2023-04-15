#pragma once

#include <windows.h>

#include "constants.h"

class DLLCONTENT MovesetLoader
{
protected:
	// Stores a handle to the shared memory
	HANDLE m_memoryHandle = nullptr;

	// Returns the name of the shared memory to look after
	virtual const TCHAR* GetSharedMemoryName() {
		return TEXT("TKMovesets2\\Mem");
	}
public:
	MovesetLoader();
	~MovesetLoader();
	
	// Main loop of the loader
	void Mainloop();
};

