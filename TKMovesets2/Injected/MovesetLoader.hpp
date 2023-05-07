#pragma once

#include <windows.h>
#include <stdio.h>
#include <map>
#include <set>
#include <polyhook2/Detour/x64Detour.hpp>
#include <polyhook2/ZydisDisassembler.hpp>
#include <Sig/Sig.hpp>

#include "InjectionUtils.hpp"
#include "GameAddressesWrapper.hpp"

#include "SharedMemory.h"
#include "constants.h"
#include "GameTypes.h"
#include "MovesetStructs.h"


struct functionHook
{
	PLH::x64Detour* detour;
	uint64_t originalAddress;
	uint64_t trampoline;
	bool isHooked = false;
};

class MovesetLoader
{
protected:
	// Stores a handle to the shared memory
	HANDLE m_memoryHandle = nullptr;
	// List of hooked functions. Do not use this directly, use CastTrampoline(), UnhookFunction(), HookFunction()
	std::map<std::string, functionHook> m_hooks;
	// Disassembler that must be passed to any hook creation
	PLH::ZydisDisassembler m_disassembler = PLH::ZydisDisassembler(PLH::Mode::x64);
	// Contains the module name
	std::string m_moduleName;
	// List of required function/variables/ hooks, will error out if one of them isn't found
	std::set<std::string> m_requiredSymbols;
	// Stores addresses of useful functions
	std::map<std::string, uint64_t> m_functions;
	// List of modules with their name, size and addresses in the current process
	std::map<std::string, moduleEntry> m_modules;

	// Returns the name of the shared memory to look after
	virtual const TCHAR* GetSharedMemoryName() = 0;
	// Sets the addresses game key 
	virtual void SetAddressesGameKey() = 0;
public:
	// Store addresses, pointers path and such from the game_addresses.ini file (or from embedded data if not found, which currently is the case)
	GameAddressesWrapper addresses;
	// Ptr to the shared memory, not casted
	SharedMemBase* orig_sharedMemPtr = nullptr;
	// List of important variable addresses that we can cache early on in there
	std::map<std::string, uint64_t> variables;
	// If set to true, forces the Mainloop() to stop
	bool mustStop = false;
	// Contains our own module address
	union {
		uint64_t moduleAddr;
		void* moduleAddrPtr;
	};
	// Size of the main module
	uint64_t moduleSize;

	// Initializes the shared memory
	bool Init();
	~MovesetLoader();

	// Cast a trampoline function and returns it
	template<class T> T CastTrampoline(const char* hookName) const;
	// Initializes a hook for a game function. Does not actually trigger it
	void InitHook(const char* hookName, uint64_t originalAddr, uint64_t newAddr);
	// Registers a function in the m_functions map, searching in a specific module for the bytes in game_addresses.ini
	void RegisterFunction(const char* functionName, const std::string& moduleName, const char* c_addressId);
	// Registers a hook in the m_hooks map, searching in a specific module for the bytes in game_addresses.ini
	void RegisterHook(const char* functionName, const std::string& moduleName, const char* c_addressId, uint64_t hookAddr);
	// Hooks a function if it exists and was successfully registered before, does nothing otherwise
	void HookFunction(const char* functionName);
	// Unhooks a function if it exists and was successfully registered before, does nothing otherwise
	void UnhookFunction(const char* functionName);
	// Returns the casted address of a previously registered function
	template<class T> T CastFunction(const char* functionName) const;
	// Returns the value of a variable, reading sizeof(T)
	template<typename T> T ReadVariable(const char* variableName) const;
	// Returns the address of the function
	uint64_t GetFunctionAddr(const char* functionName) const;
	// Returns true if a function name was registered and is callable
	bool HasFunction(const char* funcName) const {
		return m_functions.contains(funcName);
	}
	// Returns true if a hook name was registered and is callable
	bool HasHook(const char* funcName) const {
		return m_hooks.contains(funcName);
	}

	// Initializes the list of hook
	virtual void InitHooks() = 0;
	// Called upon a successful Init()
	virtual void OnInitEnd() = 0;
	// Main loop of the loader
	virtual void Mainloop() = 0;
	// Returns true if shared memory file has been successfully initialized
	bool isInitialized() const { return m_memoryHandle != nullptr; }
	// Sets the main module name and address
	void SetMainModule(const char* name);
	// Can be called by outside processes to execute an extraproperty within the game
	virtual void ExecuteExtraprop() = 0;

	// Debug function meant to be called at anytime from other processes, used for dev only
	virtual void Debug() {};
};

template<class T> T MovesetLoader::CastTrampoline(const char* hookName) const
{
	auto item = m_hooks.find(hookName);
	if (item != m_hooks.end()) {
		return (T)item->second.trampoline;
	}
	DEBUG_LOG("CastTrampoline(): Hook '%s' not found\n", hookName);
	return nullptr;
}

template<class T> T MovesetLoader::CastFunction(const char* functionName) const
{
	auto item = m_functions.find(functionName);
	if (item != m_functions.end()) {
		return (T)item->second;
	}
	DEBUG_LOG("CastFunction(): Function '%s' not found\n", functionName);
	return nullptr;
}

template<class T> T MovesetLoader::ReadVariable(const char* functionName) const
{
	auto item = variables.find(functionName);
	if (item != variables.end()) {
		return *(T*)item->second;
	}
	DEBUG_LOG("CastVariable(): Function '%s' not found\n", functionName);
	return (T)0;
}

