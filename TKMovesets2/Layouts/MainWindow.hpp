#pragma once
#include <imgui_impl_glfw.h>

// Toplevel menus
#include "NavigationMenu.hpp"
// Editor window
#include "EditorWindow.hpp"
// Submenus
#include "Submenu_Import.hpp"
#include "Submenu_Extract.hpp"
#include "Submenu_Edition.hpp"
#include "Submenu_OnlinePlay.hpp"
#include "Submenu_PersistentPlay.hpp"

// Other
#include "GameExtract.hpp"
#include "GameImport.hpp"
#include "GameSharedMem.hpp"
#include "LocalStorage.hpp"
#include "GameAddressesFile.hpp"

class MainWindow {
private:
	// If set to true, will rebuild the fonts right before the next NewFrame()
	bool m_mustRebuildFonts = false;

	// Load the fonts required for the program to work
	void LoadFonts();
	// Attempt to load a moveset, open a new editor window on success
	void LoadMovesetEditor(movesetInfo* movesetInfos);
public:
	// If true, program will exit
	bool requestedUpdate = false;

	// Navigation menu, also handles language selection and update button
	NavigationMenu navMenu;

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
	std::vector<EditorWindow*> editorWindows;

	MainWindow(GLFWwindow* window, const char* c_glsl_version);
	// Creates the required base ImGui and GLSL frames, everything we do is rendered in there
	virtual void NewFrame();
	// Main layout rendering function
	virtual void Update();
	// Cleanup function
	void Shutdown();
};
