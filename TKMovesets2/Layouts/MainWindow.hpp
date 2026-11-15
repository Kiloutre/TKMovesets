#pragma once

#include <chrono>
#include <string>
#include <vector>

// Toplevel menus
#include "NavigationMenu.hpp"
#include "SideMenu.hpp"
// Submenus
#include "Submenu_Import.hpp"
#include "Submenu_Extract.hpp"
#include "Submenu_Edition.hpp"
#include "Submenu_OnlinePlay.hpp"
#include "Submenu_PersistentPlay.hpp"
#include "Submenu_Addresses.hpp"
#include "Submenu_Extra.hpp"
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
	// Opens movesets requested from outside (Explorer) once the moveset list contains them
	void HandleOpenRequests();
	// True if the moveset file is open in one of the editor windows (same file, whatever the path spelling)
	bool IsMovesetOpenInEditor(const std::wstring& filename) const;
	struct PendingOpen { std::wstring path; std::chrono::steady_clock::time_point deadline; };
	std::vector<PendingOpen> m_pendingOpens;


	// Navigation menu, also handles language selection and update button
	NavigationMenu m_navMenu;

	// Shared address data outlives every worker and editor that borrows it.
	std::unique_ptr<GameAddressesFile> m_addrFile;

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
	// Tools > Addresses submenu
	Submenu_Addresses m_addressesMenu;
	// Tools > Extra: Tag in retail T8 Practice
	Submenu_Extra m_extraMenu;

	// Edition windows
	std::vector<std::unique_ptr<EditorVisuals>> m_editorWindows;

public:
	// If true, program will exit
    bool ConfirmExit(bool requested);
    bool m_exitPending=false, m_exitUpdate=false;
	bool requestedUpdate = false;
	// Side menu containing language selector, vsync, updater & other settings
	SideMenu sideMenu;

	explicit MainWindow(bool offline = false);
	~MainWindow();

	void SetActiveMenu(NAV__MENU menu) { m_navMenu.menuId = menu; }

	// Creates the required base ImGui and GLSL frames, everything we do is rendered in there
	void NewFrame();
	// Main layout rendering function
	void Update();
};
