#include <ImGui.h>

#include "NavigationMenu.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"

// - Const layout definition - //

const NavMenuBtn moveset_btns[] = {
	{NAV__MENU_EXTRACT, "navmenu.extraction"},
	{NAV__MENU_IMPORT, "navmenu.import"},
	{NAV__MENU_ONLINE_PLAY, "navmenu.online"},
};

const NavMenuBtn tools_btns[] = {
	{NAV__MENU_EDITION, "navmenu.moveset_edit"},
	{NAV__MENU_CAMERA, "navmenu.camera"},
};

const NavMenuBtn other_btns[] = {
	{NAV__MENU_DOCUMENTATION, "navmenu.documentation"},
	{NAV__MENU_ABOUT, "navmenu.about"},
};

// -- Private methods -- //

void NavigationMenu::RenderBtnList(const NavMenuBtn* c_btns, size_t count)
{
	for (size_t i = 0; i < count; ++i)
	{
		ImGui::Spacing();
		NavMenuBtn navBtn = c_btns[i];
		if (ImGuiExtra::RenderButtonEnabled(_(navBtn.c_name), menuId != navBtn.id, ImVec2(160, 30))) {
			menuId = navBtn.id;
		}
	}
}

// -- Public methods -- //

void NavigationMenu::Render()
{
	ImGui::SeparatorText(_("navmenu.category_moveset"));
	RenderBtnList(moveset_btns, sizeof(moveset_btns) / sizeof(*moveset_btns));

	ImGui::SeparatorText(_("navmenu.category_tools"));
	RenderBtnList(tools_btns, sizeof(tools_btns) / sizeof(*tools_btns));

	ImGui::SeparatorText(_("navmenu.category_other"));
	RenderBtnList(other_btns, sizeof(other_btns) / sizeof(*other_btns));
}