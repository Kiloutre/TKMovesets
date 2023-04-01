#include <ImGui.h>

#include "NavigationMenu.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "helpers.hpp"

// - Const layout definition - //

const NavMenuBtn cg_moveset_btns[] = {
	{NAV__MENU_EXTRACT, "navmenu.extraction"},
	{NAV__MENU_IMPORT, "navmenu.import"},
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

const LangSelect cg_languages[] = {
	{ .name = "English", .langId = "en-US" },
	{ .name = (const char*)u8"Français", .langId = "fr-FR" },
};

// -- Private methods -- //

void NavigationMenu::RenderBtnList(const NavMenuBtn* c_btns, size_t count, float width)
{
	for (size_t i = 0; i < count; ++i)
	{
		ImGui::Spacing();
		NavMenuBtn navBtn = c_btns[i];
		if (ImGuiExtra::RenderButtonEnabled(_(navBtn.c_name), menuId != navBtn.id, ImVec2(width, 30.0f))) {
			menuId = navBtn.id;
		}
	}
}

// -- Public methods -- //

NavigationMenu::NavigationMenu()
{
	const char* currentLangId = Localization::GetCurrLangId();
	for (int i = 0; i < _countof(cg_languages); ++i)
	{
		if (strcmp(cg_languages[i].langId, currentLangId) == 0) {
			m_languageId = i;
			break;
		}
	}
}

void NavigationMenu::Render(float width)
{
	ImGui::SeparatorText(_("navmenu.category_moveset"));
	RenderBtnList(cg_moveset_btns, _countof(cg_moveset_btns), width);

	ImGui::SeparatorText(_("navmenu.category_tools"));
	RenderBtnList(cg_tools_btns, _countof(cg_tools_btns), width);

	ImGui::SeparatorText(_("navmenu.category_other"));
	RenderBtnList(cg_other_btns, _countof(cg_other_btns), width);

	// Language list
	ImGui::PushItemWidth(width);
	if (ImGui::BeginCombo("##", cg_languages[m_languageId].name))
	{
		for (int i = 0; i < _countof(cg_languages); ++i)
		{
			ImGui::PushID(&i);
			if (ImGui::Selectable(cg_languages[i].name, i == m_languageId))
			{
				Localization::LoadFile(cg_languages[i].langId);
				m_languageId = i;
			}
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}
	ImGui::PopItemWidth();
}