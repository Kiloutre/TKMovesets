#pragma once

#include "Importer.hpp"
#include "GameInteraction.hpp"

#include "GameTypes.h"

struct importEntry
{
	const Byte* moveset; // Can be nullptr. If so, import from filename.
	uint64_t movesetSize; // Can be 0
	std::wstring filename;
	gameAddr playerAddress;
	ImportSettings settings;
	gameAddr* out_moveset = nullptr;
};

class GameImport : public GameInteraction
{
private:
	// Movesets (data, size) to import and corresponding player address
	std::vector<importEntry> m_plannedImportations;

protected:
	// List of errors, one extraction fail = 1 error
	std::vector<ImportationErrcode_> m_errors;
	// Importer class, never stores an Importer*, used for polymorphism
	Importer* m_importer = nullptr;
	// Importer class that is going to be freed next FreeExpiredFactoryClasses() call
	Importer* m_toFree_importer = nullptr;

	// Callback called whenever the process is re-attached
	virtual void OnProcessAttach() override;
	// Callback called whenever the process is detached
	virtual void OnProcessDetach() override;
	// Instantiate an extractor with polymorphism, also destroy the old one
	virtual void InstantiateFactory() override;
	// Function ran in the parallel thread, used to latch on to process
	virtual void RunningUpdate() override;
	// Called before detaching form the current process
	void PreProcessDetach() override;

public:
	// PlayerID to apply the moveset to
	uint8_t currentPlayerId = 0;

	// Default flags are read-only, so the importer needs this
	GameImport() { m_processExtraFlags = PROCESS_VM_OPERATION | PROCESS_VM_WRITE; }
	// Stops the thread started above
	void StopThreadAndCleanup() override;
	// Returns true if the importer  will allow an importation (false if it won't, like if characters aren't loaded)
	virtual bool CanStart(bool cached=true) const override;
	// Is currently busy with an importation
	virtual bool IsBusy() const override;
	// Queue a character importation from file
	virtual void QueueCharacterImportation(std::wstring filename, ImportSettings settings=0, gameAddr* out_moveset=nullptr);
	// Queue a character importation from moveset data.
	virtual void QueueCharacterImportation(const Byte* moveset, uint64_t movesetSize, ImportSettings settings=0, gameAddr* out_moveset = nullptr);
	virtual void QueueCharacterImportation(int playerid, const Byte* moveset, uint64_t movesetSize, ImportSettings settings=0, gameAddr* out_moveset = nullptr);
	virtual void QueueCharacterImportationOnBothPlayers(const Byte* moveset, uint64_t movesetSize, ImportSettings settings=0, gameAddr* out_moveset1 = nullptr, gameAddr* out_moveset2 = nullptr);
	// Returns an error code to consume instantly through a popup, sound player or such
	ImportationErrcode_ GetLastError();
	// Returns the moveset address of the currently selected player
	gameAddr GetCurrentPlayerMovesetAddr() const;
	// Frees the factory-obtained class we allocated earlier
	void FreeExpiredFactoryClasses() override;
};