#pragma once
#include "NavigationMenu.h"
#include <ImGui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

class MainWindow {
public:
	MainWindow(GLFWwindow* window, const char* glsl_version);
	virtual void NewFrame();
	virtual void Update();
	void Render();
	void Shutdown();

	NavigationMenu* navMenu;
};
