#pragma once

enum NAV__MENU
{
	NAV__MENU_DEFAULT = 0,

	NAV__MENU_EXTRACT = 0,
	NAV__MENU_IMPORT  = 1,
	NAV__MENU_EDITION = 2,
	NAV__MENU_ONLINE_PLAY = 3,
	NAV__MENU_CAMERA  = 4,
	NAV__MENU_ABOUT  = 5,
	NAV__MENU_DOCUMENTATION  = 6,
};

struct NavMenuBtn
{
	const NAV__MENU ID;
	const char* NAME;
	//const char* icon;
};

class NavigationMenu {
public:
	NavigationMenu();
	void Render();

	NAV__MENU menuId{ NAV__MENU_DEFAULT };
private:
	void RenderBtnList(const NavMenuBtn* BTNS, const unsigned int SIZE);
};