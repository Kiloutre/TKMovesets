#pragma once
#include <imgui_impl_glfw.h>

#include "NavigationMenu.hpp"
#include "StatusBar.hpp"
#include "Submenu_Extract.hpp"
#include "Submenu_Import.hpp"
#include "Submenu_OnlinePlay.hpp"

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
