#pragma once
#include <imgui_impl_glfw.h>

#include "NavigationMenu.hpp"
#include "StatusBar.hpp"
#include "Submenu_Extract.hpp"
#include "Submenu_Import.hpp"
#include "Submenu_OnlinePlay.hpp"
#include "GameExtract.hpp"

class MainWindow {
private:
	NavigationMenu     navMenu;
	StatusBar          statusBar;
	Submenu_Extract    extractMenu;
	Submenu_Import	   importMenu;
	Submenu_OnlinePlay onlineMenu;

	GameExtract* extractor;
public:
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
