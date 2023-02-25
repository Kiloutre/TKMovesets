#pragma once
#include <ImGui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

enum NAV__MENU
{
	NAV__MENU_DEFAULT = 0,
	NAV__MENU_EXTRACT = 0,
	NAV__MENU_IMPORT  = 1,
	NAV__MENU_EDITION = 2,
	NAV__MENU_CAMERA  = 3,
};

class NavigationMenu {
public:
	NavigationMenu();
	void Render();
	NAV__MENU getMenuId();
private:
	NAV__MENU menuId = NAV__MENU_DEFAULT;
};