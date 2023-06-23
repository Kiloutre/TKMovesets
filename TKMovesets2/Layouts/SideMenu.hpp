#pragma once

#include <thread>
#include <string>

#include "GameAddressesFile.hpp"
#include "Localization.hpp"

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
	// Currently selected lang ID
	unsigned int m_languageId = 0;
	// List of translations to display
	const TranslationData* m_translations;
	// Amount of translations
	unsigned int m_translations_count;
	// PTR to addresses file
	GameAddressesFile* m_addresses = nullptr;
	// Struct related to updating of addr & program
	s_updateStatus m_updateStatus;
	// If true, there has been an error with the update file
	bool m_updateFileInvalid = false;
	// Vsync dropdown index
	uint8_t m_vsync_setting;
	// 
	bool m_auto_update_check;
	bool m_auto_addr_update_check;

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