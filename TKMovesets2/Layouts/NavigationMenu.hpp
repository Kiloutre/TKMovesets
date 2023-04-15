#pragma once

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

struct LangSelect
{
	const char* name;
	const char* langId;
};

class NavigationMenu {
private:
	// Selected lang ID
	int m_languageId = 0;

	// Render a list of buttons with predetermined size for use in the navbar
	void RenderBtnList(const NavMenuBtn* c_btns, size_t count, float width);
public:
	// Stores the ID of the current menu to ender
	NAV__MENU menuId{ NAV__MENU_DEFAULT };

	NavigationMenu();
	// Main render function
	void Render(float width);

};