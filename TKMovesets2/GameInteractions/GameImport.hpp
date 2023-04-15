#pragma once

#include <tuple>
#include <utility>
#include <sys/stat.h>

#include "Importer.hpp"
#include "GameInteraction.hpp"

#include "GameAddresses.h"

struct importEntry
{
	const Byte* moveset; // Can be nullptr. If so, import from filename.
	uint64_t movesetSize; // Can be 0
	std::string filename;
	gameAddr playerAddress;
	ImportSettings settings;
};

class GameImport : public virtual GameInteraction
{
private:
	// Movesets (data, size) to import and corresponding player address
	std::vector<importEntry> m_plannedImportations;
	// List of errors, one extraction fail = 1 error
	std::vector<ImportationErrcode_> m_errors;

	// Callback called whenever the process is re-attached
	void OnProcessAttach() override;
	// Callback called whenever the process is detached
	void OnProcessDetach() override;
	// Instantiate an extractor with polymorphism, also destroy the old one
	void InstantiateFactory() override;
	// Function ran in the parallel thread, used to latch on to process
	void RunningUpdate() override;
	// Called before detaching form the current process
	void PreProcessDetach() override;

public:
	// Importer class, never stores an Importer*, used for polymorphism
	Importer* importer = nullptr;
	// PlayerID to apply the moveset to
	uint8_t currentPlayerId = 0;
	// Stores the in-game addresses of the last moveset loaded by the last Queue() call
	gameAddr lastLoadedMoveset = 0;

	// Default flags are read-only, so the importer needs this
	GameImport() { m_processExtraFlags = PROCESS_VM_OPERATION | PROCESS_VM_WRITE; }
	// Add flags that will be used during the next process opening
	void AddProcessFlags(DWORD flags) { m_processExtraFlags |= flags; }
	// Stops the thread started above
	void StopThreadAndCleanup() override;
	// Returns true if the extractor will allow an importation (false if it won't, like if characters aren't loaded)
	bool CanStart() override;
	// Is currently busy with an importation
	bool IsBusy() override;
	// Queue a character importation from file
	void QueueCharacterImportation(std::string filename, ImportSettings settings=0);
	// Queue a character importation from moveset data.
	void QueueCharacterImportation(const Byte* moveset, uint64_t movesetSize, ImportSettings settings=0);
	// Returns an error code to consume instantly through a popup, sound player or such
	ImportationErrcode_ GetLastError();
	// Returns the amount of characters we are able to import to
	uint8_t GetCharacterCount() override;
};