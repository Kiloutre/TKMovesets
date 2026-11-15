#pragma once

#include "ImportProfile.hpp"
#include <thread>
#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include "GameImport.hpp"
#include "LocalStorage.hpp"
#include "FileJob.hpp"
#include "AnimExtractors/AnimExtractors.hpp"

enum RenameErrcode_
{
	RenameErrcode_NoErr,
	RenameErrcode_EmptyName,
	RenameErrcode_AlreadyExists,
	RenameErrcode_RenameErr,
	RenameErrcode_InvalidName,
	RenameErrcode_OpenInEditor,
};

class Submenu_Edition
{
private:
    FileJob m_conversionJob;
    std::string m_conversionMessage;
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
	// Import settings popup (the moveset's import profile, docs/import-profile.md)
	bool m_profilePopup = false;
	ImportProfile::Profile m_profile;
	int m_profileStoryFlag = 0;
	std::string m_profileMessage;
	struct {
		// True if the thread to extract animations has been started in the past
		bool started = false;
		// Thread to extract animation
		std::thread thread;
		// Statuses of the extractions & must_interrupt
		s_animExtractionControl extractionControl;
		// Movesets
		std::vector<movesetInfo> movesets;
		// True if the extraction popup is currently shown
		bool popup = false;
		// True if we are currently extracting animations
		bool extracting = false;
		// True once Start was pressed in the currently open modal
		bool ran = false;
	} m_animExtraction;

	// Extract all animations in a thread
	void ExtractAllAnimations();
	// Render the popup used for moveset renaming
	void RenderRenamePopup();
	// Render the popup used for moveset conversion
	void RenderConversionPopup();
	// Render the popup editing a moveset's import settings
	void RenderImportProfilePopup();
	// True if the moveset file is open in an editor window, whose in-memory copy (old path, old header) would undo file-level changes
	bool IsOpenInEditor(const std::wstring& filename) const { return isOpenInEditor && isOpenInEditor(filename); }
public:
	// Set by the owner of the editor windows: true if the given moveset file is currently open in one of them
	std::function<bool(const std::wstring&)> isOpenInEditor;

	movesetInfo* Render(LocalStorage& storage);

	~Submenu_Edition();
};