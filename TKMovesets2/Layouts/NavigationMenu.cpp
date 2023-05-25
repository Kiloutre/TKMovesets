#include <ImGui.h>

#include "NavigationMenu.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "helpers.hpp"

// - Const layout definition - //

const NavMenuBtn cg_moveset_btns[] = {
	{NAV__MENU_EXTRACT, "navmenu.extraction"},
	{NAV__MENU_IMPORT, "navmenu.import"},
};

const NavMenuBtn cg_play_btns[] = {
	{NAV__MENU_PERSISTENT_PLAY, "navmenu.persistent_play"},
	{NAV__MENU_ONLINE_PLAY, "navmenu.online"},
};

const NavMenuBtn cg_tools_btns[] = {
	{NAV__MENU_EDITION, "navmenu.moveset_edit"},
	//{NAV__MENU_CAMERA, "navmenu.camera"},
};

const NavMenuBtn cg_other_btns[] = {
	//{NAV__MENU_DOCUMENTATION, "navmenu.documentation"},
	{NAV__MENU_ABOUT, "navmenu.about"},
};

// -- Private methods -- //

void NavigationMenu::RenderBtnList(const NavMenuBtn* c_btns, size_t count, float width, bool navigationLocked)
{
	for (size_t i = 0; i < count; ++i)
	{
		ImGui::Spacing();
		NavMenuBtn navBtn = c_btns[i];
		if (ImGuiExtra::RenderButtonEnabled(_(navBtn.c_name), menuId != navBtn.id && !navigationLocked, ImVec2(width, 30.0f))) {
			menuId = navBtn.id;
		}
	}
}


// -- Public methods -- //

void NavigationMenu::Render(float width, bool navigationLocked)
{
	ImGui::SeparatorText(_("navmenu.category_moveset"));
	RenderBtnList(cg_moveset_btns, _countof(cg_moveset_btns), width, navigationLocked);

	ImGui::SeparatorText(_("navmenu.category_play"));
	RenderBtnList(cg_play_btns, _countof(cg_play_btns), width, navigationLocked);

	ImGui::SeparatorText(_("navmenu.category_tools"));
	RenderBtnList(cg_tools_btns, _countof(cg_tools_btns), width, navigationLocked);

	ImGui::SeparatorText(_("navmenu.category_other"));
	RenderBtnList(cg_other_btns, _countof(cg_other_btns), width, navigationLocked);
}