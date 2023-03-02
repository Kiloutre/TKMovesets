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
	NAV__MENU id;
	const char* c_name;
	//const char* icon;
};

class NavigationMenu {
private:
	// Render a list of buttons with predetermined size for use in the navbar
	void RenderBtnList(const NavMenuBtn* c_btns, size_t count);
public:
	// Stores the ID of the current menu to ender
	NAV__MENU menuId{ NAV__MENU_DEFAULT };

	// Main render function
	void Render();

};