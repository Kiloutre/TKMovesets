#pragma once

#include <thread>
#include <string>

#include "GameAddressesFile.hpp"
#include "Localization.hpp"
#include "Keybinds.hpp"

struct s_updateStatus {
	bool error = false;
	bool addrFile = false;
	bool programUpdateAvailable = false;
	bool up_to_date = false; // up_to_date is only used to show 'Up to date' (true) text or nothing
	bool verifying = false;
	bool verifiedOnce = false; // Used to properly join the thread.
	std::string tagName;
	std::string tagNameSeparatorText;
	std::string changelog;
	std::thread thread;
};

class SideMenu
{
private:
	// List of translations to display
	const TranslationData* m_translations;
	// Settings
	struct {
		// Currently selected lang ID
		unsigned int m_languageId = 0;
		// Vsync dropdown index
		uint8_t m_vsync_setting;
		// True if the settings menu is open
		bool m_settingsMenuOpen = false;
		// Extraction: Whether to overwrite moveset extracted with similar character names or use number suffixes
		bool m_overwriteSameFilename;
		// Whether to check for program updates at startup or not
		bool m_auto_update_check;
		// Whether to download address updates at startup or not
		bool m_auto_addr_update_check;
		// If true, force the current player move to end early
		bool m_applyInstantly;
		// If true, old unused movesets will be free whenever a new one is imported
		bool m_freeUnusedMoveset;

		// >= 0 if we are currently rebinding a key. -1 is not.
		int m_rebindingIndex = -1;
		// Keys of the current ongoing binding
		s_Keybind m_currentBindingKeys;

	};
	// Amount of translations
	unsigned int m_translations_count;
	// PTR to addresses file
	GameAddressesFile* m_addresses = nullptr;
	// Struct related to updating of addr & program
	s_updateStatus m_updateStatus;
	// If true, there has been an error with the update file
	bool m_updateFileInvalid = false;
	// List of keybinds
	s_Keybinds* m_keybinds;

	// Render the changelog that displays when a new update has been detected
	void RenderChangelog();
	// Render the settings menu 
	void RenderSettingsMenu();
	// Start a thread that will check for updates
	void RequestCheckForUpdates();
	// Make a HTTP request to check for a possible new release or more
	void CheckForUpdates(bool firstTime);
public:
	// Ptr to MainProgram.requestedUpdate, set to true when updating in order to close the program
	bool* requestedUpdatePtr = nullptr;

	SideMenu();
	~SideMenu();

	// Main render function
	void Render(float width);
	// Set addr file, used for update checking
	void SetAddrFile(GameAddressesFile* addresses);
	// Cleanup the HTTP request-making thread
	void CleanupThread();
};