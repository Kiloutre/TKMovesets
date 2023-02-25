#include "NavigationMenu.h"

NavMenuBtn navBtns[] = {
	{NAV__MENU_EXTRACT, "Extraction"},
	{NAV__MENU_IMPORT, "Importation"},
	{NAV__MENU_EDITION, "Edition"},
	{NAV__MENU_CAMERA, "Camera"},
};

NavigationMenu::NavigationMenu()
{
	menuId = NAV__MENU_DEFAULT;
}

NAV__MENU NavigationMenu::getMenuId()
{
	return menuId;
}

void NavigationMenu::Render()
{
	for (unsigned int i = 0; i < (sizeof(navBtns) / sizeof(NavMenuBtn)); ++i)
	{
		NavMenuBtn navBtn = navBtns[i];
		ImGui::Spacing();
		if (menuId == navBtn.id)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			ImGui::Button(navBtn.name, ImVec2(230 - 15, 39));
			ImGui::PopStyleVar();
		}
		else if (ImGui::Button(navBtn.name, ImVec2(230 - 15, 39)))
			menuId = navBtn.id;
	}
}