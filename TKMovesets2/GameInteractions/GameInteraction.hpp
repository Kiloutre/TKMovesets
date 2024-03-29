#pragma once

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "LocalStorage.hpp"
#include "ThreadedClass.hpp"

#include "Games.hpp"

#include "GameTypes.h"

class GameInteraction : public ThreadedClass
{
protected:
	// Flags that will be used to open the process with
	DWORD m_processExtraFlags = 0;
	// Last status of the process, used to detect status change and trigger appropriate events
	GameProcessErrcode_ m_lastStatus = GameProcessErrcode_PROC_NOT_ATTACHED;
	// Variable containing whether or not we can start our program's action, updated every thread loop
	bool m_canStart = false;

	// Instantiate a class with polymorphism, also destroy the old one
	virtual void InstantiateFactory() = 0;
	// Function ran in the parallel thread, executed by Update() only if latched on and if CanStart() returned true
	virtual void RunningUpdate() = 0;
	// Callback called when the process has latched on a process for the first time
	virtual void OnProcessAttach() { };
	// Callback called when the process has detached, on purpose or not (status change from ATTACHED to any other)
	virtual void OnProcessDetach() { };
	// Called before detaching from the process when executing SetTargetProces()
	virtual void PreProcessDetach() { };

	// Function ran in the parallel thread, executed regularly
	void Update();
public:
	// Everything that is related to files is done through .storage. Listing, deleting, etc. We store this here because GameData* needs a storage copy anyway.
	LocalStorage* storage = nullptr;
	// Stores the currently selected game's info
	GameInfo const* currentGame = nullptr;
	// Used to know which process is currently being searched/opened
	std::string currentGameProcess;
	// Currently opened process for extraction.
	GameProcess process;
	// Helper to read on address from their game_addresses.txt identifier
	GameData game;
	// Progress of the current task (import/export), between 0 and 100. 
	uint8_t progress = 0;
	// Timestamp of the current action's start, currently unused but can be used to implement timeout on actions. TODO
	uint64_t actionStartDate = 0;

	// Returns true if process is attached
	bool IsAttached() const;
	// Returns the amount of characters we are able to interact with
	uint8_t GetCharacterCount() const;
	// Returns true if the extractor will allow an extraction (false if it won't, like if characters aren't loaded)
	virtual bool CanStart(bool cached=true) const = 0;
	// Is currently busy
	virtual bool IsBusy() const = 0;
	// Set the process to open
	void SetTargetProcess(const GameInfo* gameInfo);
	// Inits the member that needs to be created (.process, .game). Those Must be deleted afterward.
	void Init(GameAddressesFile* addrFile, LocalStorage* t_storage);
	// Destroys the thread
	virtual void StopThreadAndCleanup();
	// Frees the factory-obtained class we allocated earlier
	virtual void FreeExpiredFactoryClasses() = 0;
	// Deattach the current process if attached
	virtual void ResetTargetProcess() {};
};
