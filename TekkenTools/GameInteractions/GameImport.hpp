#pragma once

#include <pair>
#include <sys/stat.h>

//#include "Importer.hpp"
#include "LocalStorage.hpp"
#include "GameInteraction.hpp"

#include "GameAddresses.h"

class GameImport : public virtual GameInteraction
{
private:
	// Importer class, never stores an Importer*, used for polymorphism
	void* m_importer = nullptr;
	// Movesets to import and corresponding player id 
	std::vector<std::pair<std::string, gameAddr>> m_plannedImportations;

	// Callback called whenever the process is re-atached
	void OnProcessAttach() override;
	// Instantiate an extractor with polymorphism, also destroy the old one
	void InstantiateFactory() override;
	// Function ran in the parallel thread, used to latch on to process
	void RunningUpdate() override;
public:
	// Stops the thread started above
	void StopThreadAndCleanup() override;
	// Returns true if the extractor will allow an extraction (false if it won't, like if characters aren't loaded)
	bool CanStart() override;
	// Is currently busy with an extraction
	bool IsBusy();
	// Queue a character extraction. -1 of all characters
	void QueueCharacterImportation(const char* filename, int playerId);
};