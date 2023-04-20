#pragma once

#include <windows.h>
#include <stdio.h>
#include <map>
#include <set>
#include <polyhook2/Detour/x64Detour.hpp>
#include <polyhook2/ZydisDisassembler.hpp>
#include <Sig/Sig.hpp>

#include "InjectionUtils.hpp"
#include "GameAddressesFile.hpp"

#include "constants.h"
#include "SharedMemory.h"


typedef unsigned char Byte;

struct functionHook
{
	PLH::x64Detour* detour;
	uint64_t trampoline;
};

class MovesetLoader
{
protected:
	// Stores a handle to the shared memory
	HANDLE m_memoryHandle = nullptr;
	// Ptr to the shared memory
	SharedMemory* m_sharedMemPtr = nullptr;
	// List of hooked functions
	std::map<std::string, functionHook> m_hooks;
	// Disassembler that must be passed to any hook creation
	PLH::ZydisDisassembler m_disassembler = PLH::ZydisDisassembler(PLH::Mode::x64);
	// Contains our own module address
	union {
		uint64_t m_moduleAddr;
		void* m_moduleAddrPtr;
	};
	// Size of the main module
	uint64_t m_moduleSize;
	// Contains the module name
	std::string m_moduleName;
	// List of required hooks,will error out if one of them isn't found
	std::set<std::string> m_requiredHooks;

	// Returns the name of the shared memory to look after
	virtual const TCHAR* GetSharedMemoryName() = 0;
public:
	// Store addresses, pointers path and such from the game_addresses.ini file (or from embedded data if not found, which currently is the case)
	GameAddressesFile addresses;

	// Initializes the shared memory
	bool Init();
	~MovesetLoader();

	// Cast a trampoline function and returns it
	template<class T> T CastTrampoline(std::string hookName);
	// Initializes a hook for a game function. Does not actually trigger it
	void InitHook(const char* hookName, uint64_t originalAddr, uint64_t newAddr);

	// Initializes the list of hook
	virtual void InitHooks() = 0;
	// Called upon a successful Init()
	virtual void PostInit() = 0;
	// If set to true, force the Mainloop() to stop
	bool mustStop = false;
	// Main loop of the loader
	void Mainloop();
	// Returns true if shared memory file has been successfully initialized
	bool isInitialized() { return m_memoryHandle != nullptr; }
	// Sets the main module name and address
	void SetMainModule(const char* name) {
		m_moduleName = name;
		InjectionUtils::GetSelfModuleInfos(name, m_moduleAddr, m_moduleSize);
	}
};

template<class T> T MovesetLoader::CastTrampoline(std::string hookName)
{
	if (m_hooks.contains(hookName)) {
		return (T)m_hooks[hookName].trampoline;
	}
	DEBUG_LOG("CastTrampoline(): Hook '%s' not found\n", hookName.c_str());
	return nullptr;
}