#pragma once

#include <windows.h>
#include <stdio.h>
#include <polyhook2/Detour/x64Detour.hpp>
#include <polyhook2/ZydisDisassembler.hpp>

#include "constants.h"
#include "SharedMemory.h"

typedef unsigned char Byte;

struct functionHook
{
	PLH::x64Detour* detour;
	std::string name;
};

class MovesetLoader
{
protected:
	// Stores a handle to the shared memory
	HANDLE m_memoryHandle = nullptr;
	// Ptr to the shared memory
	SharedMemory* m_sharedMemPtr = nullptr;
	// List of hooked functions
	std::vector<functionHook> m_hooks;
	// Disassembler that must be passed to any hook creation
	PLH::ZydisDisassembler m_disassembler = PLH::ZydisDisassembler(PLH::Mode::x64);
	// Contains our own module address
	uint64_t m_moduleAddr;

	// Returns the name of the main module of the game
	virtual const char* GetMainModuleName() = 0;
	// Returns the name of the shared memory to look after
	virtual const TCHAR* GetSharedMemoryName() = 0;
public:
	// Initializes the shared memory
	bool Init();
	~MovesetLoader();

	MovesetLoader* GetInstance();

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
