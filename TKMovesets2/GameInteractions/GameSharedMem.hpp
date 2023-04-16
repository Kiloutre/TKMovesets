#pragma once

#include "Online.hpp"
#include "GameImport.hpp"

struct onlineImportEntry
{
	movesetInfo moveset;
	ImportSettings settings;
	unsigned int playerId;
};

class GameSharedMem : public GameImport
{
private:
	// Movesets (data, size) to import and corresponding player address
	std::vector<onlineImportEntry> m_plannedImportations;

	// Callback called whenever the process is re-attached
	void OnProcessAttach() override;
	// Instantiate an extractor with polymorphism, also destroy the old one
	void InstantiateFactory() override;
	// Function ran in the parallel thread, used to latch on to process and load the shared memory
	void RunningUpdate() override;

public:
	// Online class, manages shared memory
	Online* sharedMemHandler = nullptr;

	// Default flags are read-only, so the importer needs this
	GameSharedMem() { m_processExtraFlags = PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD; }
	// Always true because SharedMem can function as long as the process is started. Condition used to trigger RunningUpdate()
	bool CanStart() override { return true; };
	// Is currently busy with an importation
	bool IsBusy() override;

	void QueueCharacterImportation(movesetInfo* moveset, unsigned int playerId, ImportSettings settings = 0);

	// Forbid access to these
	void QueueCharacterImportation(std::string filename, ImportSettings settings = 0) override { throw; };
	void QueueCharacterImportation(const Byte* moveset, uint64_t movesetSize, ImportSettings settings = 0) override { throw; };
};
