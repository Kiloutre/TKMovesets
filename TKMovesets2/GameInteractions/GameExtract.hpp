#pragma once

#include <algorithm>

#include "Extractor.hpp"
#include "GameInteraction.hpp"

#include "GameTypes.h"

class GameExtract : public virtual GameInteraction
{
private:
	// Extractor class, never stores an Extractor*, used for polymorphism
	Extractor* m_extractor = nullptr;
	// Player addresses to extract and their respective extraction settings
	std::vector<std::pair<gameAddr, ExtractSettings>> m_extractionQueue;
	// List of errors, one extraction fail = 1 error
	std::vector<ExtractionErrcode_> m_errors;

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

	// Stops the thread started above
	void StopThreadAndCleanup() override;
	// Returns true if the extractor will allow an extraction (false if it won't, like if characters aren't loaded)
	bool CanStart(bool cached=true) override;
	// Is currently busy with an extraction
	bool IsBusy() override;
	// Queue a character extraction. -1 of all characters
	void QueueCharacterExtraction(int playerId, ExtractSettings settings=0);
	// Returns an error code to consume instantly through a popup, sound player or such
	ExtractionErrcode_ GetLastError();
	// Returns the amount of characters we are able to import to
	uint8_t GetCharacterCount() override;
};