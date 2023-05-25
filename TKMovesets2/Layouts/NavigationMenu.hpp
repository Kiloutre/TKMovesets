#pragma once

#include "Localization.hpp"

enum NAV__MENU
{
	NAV__MENU_EXTRACT,
	NAV__MENU_IMPORT,
	NAV__MENU_EDITION,
	NAV__MENU_PERSISTENT_PLAY,
	NAV__MENU_ONLINE_PLAY,
	NAV__MENU_CAMERA,
	NAV__MENU_ABOUT,
	NAV__MENU_DOCUMENTATION,
    
	NAV__MENU_DEFAULT = NAV__MENU_EXTRACT
};

struct NavMenuBtn
{
	NAV__MENU id;
	const char* c_name;
	//const char* icon;
};


class NavigationMenu
{
private:
	// Render a list of buttons with predetermined size for use in the navbar
	void RenderBtnList(const NavMenuBtn* c_btns, size_t count, float width, bool navigationLocked);
public:
	// Stores the ID of the current menu to render
	NAV__MENU menuId{ NAV__MENU_DEFAULT };

	// Main render function
	void Render(float width, bool navigationLocked);
};