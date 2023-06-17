#pragma once

#include <thread>

#include "GameImport.hpp"
#include "LocalStorage.hpp"
#include "AnimExtractors/AnimExtractors.hpp"

enum RenameErrcode_
{
	RenameErrcode_NoErr,
	RenameErrcode_EmptyName,
	RenameErrcode_AlreadyExists,
	RenameErrcode_RenameErr,
	RenameErrcode_InvalidName,
};

class Submenu_Edition
{
private:
	// Current error message in rename popup
	RenameErrcode_ currentErr = RenameErrcode_NoErr;
	// Stores whether the rename popup is open or not
	bool m_renamePopup = false;
	// Stores whether the conversion popup is open or not
	bool m_conversionPopup = false;
	// Name of the moveset to apply in the rename popup
	char m_newName[FILENAME_MAX];
	// Moveset to rename/convert in the popups
	movesetInfo m_actionTarget;
	struct {
		// True if the thread to extract animations has been started in the past
		bool started = false;
		// Thread to extract animation
		std::thread thread;
		// Statuses of the extractions
		std::vector<s_extractionStatus> statuses;
		// Movesets
		std::vector<movesetInfo> movesets;
		// True if the extraction popup is currently shown
		bool popup = false;
		// True if we are currently extracting animations
		bool extracting = false;
	} m_animExtraction;

	// Extract all animations in a thread
	void ExtractAllAnimations();
	// Render the popup used for moveset renaming
	void RenderRenamePopup();
	// Render the popup used for moveset conversion
	void RenderConversionPopup();
public:
	movesetInfo* Render(LocalStorage& storage);

	~Submenu_Edition();
};