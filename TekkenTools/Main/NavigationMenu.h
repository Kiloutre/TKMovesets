#pragma once
#include <ImGui.h>

enum NAV__MENU
{
	NAV__MENU_DEFAULT = 0,
	NAV__MENU_EXTRACT = 0,
	NAV__MENU_IMPORT  = 1,
	NAV__MENU_EDITION = 2,
	NAV__MENU_CAMERA  = 3,
};

typedef struct NavMenuBtn
{
	const NAV__MENU id;
	const char* name;
	//const char* icon;
};

class NavigationMenu {
public:
	NavigationMenu();
	void Render();
	NAV__MENU getMenuId();
private:
	NAV__MENU menuId = NAV__MENU_DEFAULT;
};