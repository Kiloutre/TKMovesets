#pragma once

#include <string>

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "LocalStorage.hpp"
#include "ThreadedClass.hpp"

#include "Games.hpp"

#include "GameAddresses.h"

class GameInteraction : public ThreadedClass
{
protected:
	// Flags that will be used to open the process with
	DWORD m_processExtraFlags = 0;

	// Instantiate an extractor with polymorphism, also destroy the old one
	virtual void InstantiateFactory() = 0;
	// Function ran in the parallel thread, executed by Update() only if latched on and if CanStart() returned true
	virtual void RunningUpdate() = 0;
	// Callback called when the process has latched on a process for the first time
	virtual void OnProcessAttach() = 0;
	// Called before detaching from the process when executing SetTargetProces()
	virtual void PreProcessDetach() {};

	// Function ran in the parallel thread, executed regularly
	void Update();
public:
	// Everything that is related to files is done through .storage. Listing, deleting, etc.
	LocalStorage* storage = nullptr;
	// Stores the ID of the currently opened game
	int8_t currentGameId = -1;
	// Used to know which process is currently being searched/opened
	std::string currentGameProcess;
	// Currently opened process for extraction
	GameProcess* process = nullptr;
	// Helper to read on address from their game_addresses.txt identifier
	GameData* game = nullptr;
	// Progress of the current task, between 0 and 100
	uint8_t progress = 0;

	// Returns the amount of characters we are able to interact with
	virtual uint8_t GetCharacterCount() = 0;
	// Returns true if the extractor will allow an extraction (false if it won't, like if characters aren't loaded)
	virtual bool CanStart() = 0;
	// Is currently busy
	virtual bool IsBusy() = 0;
	// Set the process to open
	void SetTargetProcess(const char* processName, uint8_t gameId);
	// Inits the member that needs to be created (.process, .game). Those Must be deleted afterward.
	void Init(GameAddressesFile* addrFile, LocalStorage* t_storage);
};