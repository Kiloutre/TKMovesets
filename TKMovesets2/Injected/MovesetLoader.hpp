#pragma once

#include <windows.h>
#include <stdio.h>

#include "constants.h"
#include "SharedMemory.h"

typedef unsigned char Byte;

class MovesetLoader
{
protected:
	// Stores a handle to the shared memory
	HANDLE m_memoryHandle = nullptr;
	// Ptr to the shared memory
	SharedMemory* m_sharedMemPtr = nullptr;

	// Returns the name of the shared memory to look after
	virtual const TCHAR* GetSharedMemoryName() = 0;
public:
	// Initializes the shared memory
	bool Init();
	~MovesetLoader();

	// Called once shared memory has been successfully loaded
	virtual void PostInit() = 0;
	// If set to true, force the Mainloop() to stop
	bool mustStop = false;
	// Main loop of the loader
	void Mainloop();
	// Returns true if shared memory file has been successfully initialized
	bool isInitialized() {
		return m_memoryHandle != nullptr;
	}
};
