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

// Other
#include "GameExtract.hpp"
#include "GameImport.hpp"
#include "LocalStorage.hpp"
#include "GameAddressesFile.hpp"

class MainWindow {
private:
	NavigationMenu navMenu;

	// Attempt to load a moveset, open a new editor window on success
	void LoadMoveset(movesetInfo* movesetInfos);
public:
	// Extractor instance, can attach to its own process separately from everyone else
	GameExtract extractor;
	// Importer instance, can attach to its own process separately from everyone else
	GameImport importer;
	// Storage instance, use to access local moveset informations
	LocalStorage storage;
	// Addrfile to pass on child classes (mainly editorWindows)
	GameAddressesFile* addrFile = nullptr;

	// Extraction submenu
	Submenu_Extract extractMenu;
	// Importation submenu
	Submenu_Import importMenu;
	// Edition submenu
	Submenu_Edition editionMenu;
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
