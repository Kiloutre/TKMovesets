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
	// List of required function and hooks,will error out if one of them isn't found
	std::set<std::string> m_requiredFunctions;
	// Stores addresses of useful functions
	std::map<std::string, uint64_t> m_functions;
	// List of modules with their name, size and addresses in the current process
	std::map<std::string, moduleEntry> m_modules;

	// Returns the name of the shared memory to look after
	virtual const TCHAR* GetSharedMemoryName() = 0;
public:
	// Store addresses, pointers path and such from the game_addresses.ini file (or from embedded data if not found, which currently is the case)
	GameAddressesFile addresses;

	// Initializes the shared memory
	bool Init();
	~MovesetLoader();

	// Cast a trampoline function and returns it
	template<class T> T CastTrampoline(const std::string& hookName);
	// Initializes a hook for a game function. Does not actually trigger it
	void InitHook(const char* hookName, uint64_t originalAddr, uint64_t newAddr);
	// Registers a function in the m_functions map, searching in a specific module for the bytes in game_addresses.ini
	void RegisterFunction(const char* functionName, const char* moduleName, const char* c_addressId);
	// Registers a hook in the m_hooks map, searching in a specific module for the bytes in game_addresses.ini
	void RegisterHook(const char* functionName, const char* moduleName, const char* c_addressId, uint64_t hookAddr);
	// Returns the casted address of a previously registered fnction
	template<class T> T CastFunction(const std::string& functionName);

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
	void SetMainModule(const char* name);
};

template<class T> T MovesetLoader::CastTrampoline(const std::string& hookName)
{
	if (m_hooks.contains(hookName)) {
		return (T)m_hooks[hookName].trampoline;
	}
	DEBUG_LOG("CastTrampoline(): Hook '%s' not found\n", hookName.c_str());
	return nullptr;
}

template<class T> T MovesetLoader::CastFunction(const std::string& functionName)
{
	if (m_functions.contains(functionName)) {
		return (T)m_functions[functionName];
	}
	DEBUG_LOG("CastFunction(): Function '%s' not found\n", functionName.c_str());
	return nullptr;
}