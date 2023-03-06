#pragma once

#include "Extractor.hpp"
#include "GameInteraction.hpp"

#include "GameAddresses.h"

class GameExtract : public virtual GameInteraction
{
private:
	// Extractor class, never stores an Extractor*, used for polymorphism
	Extractor* m_extractor = nullptr;
	// Player address to extract
	std::vector<gameAddr> m_playerAddress;
	// List of errors, one extraction fail = 1 error
	std::vector<ExtractionErrcode> m_errors;

	// Reads the movesets for the players characters' names. Accessible under .characterNames
	void LoadCharacterNames();
	// Callback called whenever the process is re-atached
	void OnProcessAttach() override;
	// Instantiate an extractor with polymorphism, also destroy the old one
	void InstantiateFactory() override;
	// Function ran in the parallel thread, executed only if latched on and if CanStart() returned true
	void RunningUpdate() override;
public:
	// Contains the character names
	std::string characterNames[4];
	// Determine whether to ovewrite existing movesets when extracting or to append a number to the name
	bool overwriteSameFilename = false;

	// Stops the thread started above
	void StopThreadAndCleanup() override;
	// Returns true if the extractor will allow an extraction (false if it won't, like if characters aren't loaded)
	bool CanStart() override;
	// Is currently busy with an extraction
	bool IsBusy() override;
	// Queue a character extraction. -1 of all characters
	void QueueCharacterExtraction(int playerId);
	// Returns an error code to consume instantly through a popup, sound player or such
	ExtractionErrcode GetLastError();
};