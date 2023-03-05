#include <ImGui.h>

#include "NavigationMenu.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"

// - Const layout definition - //

const NavMenuBtn cg_moveset_btns[] = {
	{NAV__MENU_EXTRACT, "navmenu.extraction"},
	{NAV__MENU_IMPORT, "navmenu.import"},
	{NAV__MENU_ONLINE_PLAY, "navmenu.online"},
};

const NavMenuBtn cg_tools_btns[] = {
	{NAV__MENU_EDITION, "navmenu.moveset_edit"},
	{NAV__MENU_CAMERA, "navmenu.camera"},
};

const NavMenuBtn cg_other_btns[] = {
	{NAV__MENU_DOCUMENTATION, "navmenu.documentation"},
	{NAV__MENU_ABOUT, "navmenu.about"},
};

const LangSelect cg_languages[] = {
	{ .name = "English", .filename = "en_US" },
	{ .name = (const char*)u8"Français", .filename = "fr_FR" },
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
	RenderBtnList(cg_moveset_btns, sizeof(cg_moveset_btns) / sizeof(*cg_moveset_btns));

	ImGui::SeparatorText(_("navmenu.category_tools"));
	RenderBtnList(cg_tools_btns, sizeof(cg_tools_btns) / sizeof(*cg_tools_btns));

	ImGui::SeparatorText(_("navmenu.category_other"));
	RenderBtnList(cg_other_btns, sizeof(cg_other_btns) / sizeof(*cg_other_btns));

	// Language list
	ImGui::PushItemWidth(160);
	if (ImGui::BeginCombo("##", cg_languages[m_languageId].name))
	{
		for (size_t i = 0; i < sizeof(cg_languages) / sizeof(*cg_languages); ++i)
		{
			ImGui::PushID(&i);
			if (ImGui::Selectable(cg_languages[i].name, i == m_languageId))
			{
				Localization::LoadFile(cg_languages[i].filename);
				m_languageId = i;
			}
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}
}