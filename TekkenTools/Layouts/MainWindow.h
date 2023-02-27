#pragma once
#include <imgui_impl_glfw.h>

#include "NavigationMenu.h"
#include "StatusBar.h"
#include "Submenu_Extract.h"
#include "Submenu_Import.h"
#include "Submenu_OnlinePlay.h"

class MainWindow {
public:
	MainWindow(GLFWwindow* window, const char* GLSL_VERSION);
	virtual void NewFrame();
	virtual void Update(const int WIDTH, const int HEIGHT);
	void Render();
	void Shutdown();

	NavigationMenu     navMenu;
	StatusBar          statusBar;
	Submenu_Extract    extractMenu;
	Submenu_Import	   importMenu;
	Submenu_OnlinePlay onlineMenu;
};
