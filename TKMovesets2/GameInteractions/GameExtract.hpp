#pragma once

#include <memory>

#include "Extractor.hpp"
#include "GameInteraction.hpp"

#include "GameTypes.h"

class GameExtract : public virtual GameInteraction
{
private:
	// Player addresses to extract and their respective extraction settings
	std::vector<std::pair<gameAddr, ExtractSettings>> m_extractionQueue;
	// List of errors, one extraction fail = 1 error
	std::vector<std::pair<ExtractionErrcode_, std::string>> m_errors;
	// Owns the active game-specific extractor.
	std::unique_ptr<Extractor> m_extractor;

	// Reads the movesets for the players characters' names. Accessible under .characterNames
	void LoadCharacterNames();
	// Callback called whenever the process is re-atached
	void OnProcessAttach() override;
	// Instantiate an extractor with polymorphism, also destroy the old one
	void InstantiateFactory() override;
	// Function ran in the parallel thread, executed only if latched on and if CanStart() returned true
	void RunningUpdate() override;
public:
	~GameExtract() override { StopThreadAndCleanup(); }
	// Contains the characters to extract's names 
	std::string characterNames[4];

	// Stops the thread started above
	void StopThreadAndCleanup() override;
	// Returns true if the extractor will allow an extraction (false if it won't, like if characters aren't loaded)
	bool CanStart(bool cached=true) const override;
	// Is currently busy with an extraction
	bool IsBusy() const override;
	// Queue a character extraction. -1 of all characters
	void QueueCharacterExtraction(int playerId, ExtractSettings settings=0);
	// Returns an error code to consume instantly through a popup, sound player or such.
	// [detail] receives the extractor's own message when it has one (empty otherwise).
	ExtractionErrcode_ GetLastError(std::string* detail = nullptr);
	// Frees the factory-obtained class we allocated earlier
	void FreeExpiredFactoryClasses() override;
};