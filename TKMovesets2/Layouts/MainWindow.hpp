#pragma once
#include <imgui_impl_glfw.h>

// Toplevel menus
#include "NavigationMenu.hpp"
#include "StatusBar.hpp"
// Submenus
#include "Submenu_OnlinePlay.hpp"

// Other
#include "GameExtract.hpp"
#include "GameImport.hpp"
#include "LocalStorage.hpp"

class MainWindow {
private:
	NavigationMenu     navMenu;
	//StatusBar          statusBar;
	Submenu_OnlinePlay onlineMenu;
public:
	GameExtract extractor;
	GameImport importer;
	LocalStorage storage;

	MainWindow(GLFWwindow* window, const char* c_glsl_version);
	// Creates the required base ImGui and GLSL frames, everything we do is rendered in there
	virtual void NewFrame();
	// Main layout rendering function
	virtual void Update(int width, int height);
	// Makes the final ImGui and GLSL calls to display the window's content
	void Render();
	// Cleanup function
	void Shutdown();
};
