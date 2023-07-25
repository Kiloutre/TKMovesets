#pragma once

// Toplevel menus
#include "NavigationMenu.hpp"
#include "SideMenu.hpp"
// Submenus
#include "Submenu_Import.hpp"
#include "Submenu_Extract.hpp"
#include "Submenu_Edition.hpp"
#include "Submenu_OnlinePlay.hpp"
#include "Submenu_PersistentPlay.hpp"
// Editor window
#include "EditorVisuals.hpp"

// Other
#include "GameExtract.hpp"
#include "GameImport.hpp"
#include "GameSharedMem.hpp"
#include "LocalStorage.hpp"
#include "GameAddressesFile.hpp"

class MainWindow {
private:
	// Attempt to load a moveset, open a new editor window on success
	void LoadMovesetEditor(const movesetInfo* movesetInfos);


	// Navigation menu, also handles language selection and update button
	NavigationMenu m_navMenu;

	// Extractor instance, can attach to its own process separately from everyone else
	GameExtract m_extractor;
	// Importer instance, can attach to its own process separately from everyone else
	GameImport m_importer;
	// Shared mem instance, can attach to its own process, manage shared memory and import movesets
	GameSharedMem m_sharedMem;
	// Storage instance, use to access local moveset list
	LocalStorage m_storage;

	// Extraction submenu
	Submenu_Extract m_extractMenu;
	// Importation submenu
	Submenu_Import m_importMenu;
	// Edition submenu
	Submenu_Edition m_editionMenu;
	// Online submenu
	Submenu_OnlinePlay m_onlineMenu;
	// Persistent play submenu
	Submenu_PersistentPlay m_persistentPlayMenu;

	// Edition windows
	std::vector<EditorVisuals*> m_editorWindows;

	// Addrfile to pass to child classes (mainly editorWindows)
	GameAddressesFile* m_addrFile = nullptr;
public:
	// If true, program will exit
	bool requestedUpdate = false;
	// Side menu containing language selector, vsync, updater & other settings
	SideMenu sideMenu;

	MainWindow();
	~MainWindow();

	// Creates the required base ImGui and GLSL frames, everything we do is rendered in there
	void NewFrame();
	// Main layout rendering function
	void Update();
	// Cleanup function
	void Shutdown();
};
