#pragma once
#include <imgui_impl_glfw.h>

// Toplevel menus
#include "NavigationMenu.hpp"
//#include "StatusBar.hpp"
// Submenus
#include "Submenu_Import.hpp"
#include "Submenu_Extract.hpp"
#include "Submenu_Edition.hpp"
#include "Submenu_OnlinePlay.hpp"

// Other
#include "GameExtract.hpp"
#include "GameImport.hpp"
#include "LocalStorage.hpp"

class MainWindow {
private:
	NavigationMenu navMenu;
	//StatusBar          statusBar;
	//Submenu_OnlinePlay onlineMenu;
public:
	// Extractor instance, can attach to its own process separately from everyone else
	GameExtract extractor;
	// Importer instance, can attach to its own process separately from everyone else
	GameImport importer;
	// Storage instance, use to access local moveset informations
	LocalStorage storage;

	// Extraction menu
	Submenu_Extract extractMenu;

	// Importation menu
	Submenu_Import importMenu;

	// Edition menu
	Submenu_Edition editionMenu;

	MainWindow(GLFWwindow* window, const char* c_glsl_version);
	// Creates the required base ImGui and GLSL frames, everything we do is rendered in there
	virtual void NewFrame();
	// Main layout rendering function
	virtual void Update();
	// Makes the final ImGui and GLSL calls to display the window's content
	void Render();
	// Cleanup function
	void Shutdown();
};
