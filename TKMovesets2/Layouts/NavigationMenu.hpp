#pragma once

#include <thread>

#include "Localization.hpp"
#include "GameAddressesFile.hpp"

enum NAV__MENU
{
	NAV__MENU_EXTRACT,
	NAV__MENU_IMPORT,
	NAV__MENU_EDITION,
	NAV__MENU_PERSISTENT_PLAY,
	NAV__MENU_ONLINE_PLAY,
	NAV__MENU_CAMERA,
	NAV__MENU_ABOUT,
	NAV__MENU_DOCUMENTATION,
    
	NAV__MENU_DEFAULT = NAV__MENU_EXTRACT
};

struct NavMenuBtn
{
	NAV__MENU id;
	const char* c_name;
	//const char* icon;
};

class NavigationMenu
{
private:
	// Currently selected lang ID
	unsigned int m_languageId = 0;
	// List of translations to display
	TranslationData* m_translations;
	// Amount of translations
	unsigned int m_translations_count;
	// PTR to addresses file
	GameAddressesFile* m_addresses = nullptr;
	// Struct related to updating of addr & program
	struct {
		bool addrFile = false;
		bool programUpdateAvailable = false;
		bool up_to_date = false; // up_to_date is only used to show 'Up to date' (true) text or nothing
		bool verifying = false;
		bool verifiedOnce = false; // Used to properly join the thread.
		std::thread thread;
	} m_updateStatus;

	// Start a thread that will check for updates
	void RequestCheckForUpdates();
	// Make a HTTP request to check for a possible new release or more
	void CheckForUpdates();
	// Render a list of buttons with predetermined size for use in the navbar
	void RenderBtnList(const NavMenuBtn* c_btns, size_t count, float width, bool navigationLocked);
public:
	// Stores the ID of the current menu to render
	NAV__MENU menuId{ NAV__MENU_DEFAULT };

	NavigationMenu();
	~NavigationMenu();

	// Main render function
	void Render(float width, bool navigationLocked);
	// Set addr file, used for update checking
	void SetAddrFile(GameAddressesFile* addresses);
};