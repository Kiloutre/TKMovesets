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
public:
	// If true, program will exit
	bool requestedUpdate = false;

	// Navigation menu, also handles language selection and update button
	NavigationMenu navMenu;
	// Side menu containing language selector, vsync, updater & other settings
	SideMenu sideMenu;

	// Extractor instance, can attach to its own process separately from everyone else
	GameExtract extractor;
	// Importer instance, can attach to its own process separately from everyone else
	GameImport importer;
	// Shared mem instance, can attach to its own process, manage shared memory and import movesets
	GameSharedMem sharedMem;
	// Storage instance, use to access local moveset list
	LocalStorage storage;

	// Addrfile to pass to child classes (mainly editorWindows)
	GameAddressesFile* addrFile = nullptr;

	// Extraction submenu
	Submenu_Extract extractMenu;
	// Importation submenu
	Submenu_Import importMenu;
	// Edition submenu
	Submenu_Edition editionMenu;
	// Online submenu
	Submenu_OnlinePlay onlineMenu;
	// Persistent play submenu
	Submenu_PersistentPlay persistentPlayMenu;

	// Edition windows
	std::vector<EditorVisuals*> editorWindows;

	MainWindow();
	// Creates the required base ImGui and GLSL frames, everything we do is rendered in there
	virtual void NewFrame();
	// Main layout rendering function
	virtual void Update();
	// Cleanup function
	void Shutdown();
};
