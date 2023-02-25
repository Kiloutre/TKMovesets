#include "NavigationMenu.h"

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
	ImGui::Spacing();
	if (ImGui::Button(" Extraction", ImVec2(230 - 15, 39)))
		menuId = NAV__MENU_EXTRACT;

	ImGui::Spacing();
	if (ImGui::Button(" Importation", ImVec2(230 - 15, 39)))
		menuId = NAV__MENU_IMPORT;

	ImGui::Spacing();
	if (ImGui::Button(" Edition", ImVec2(230 - 15, 39)))
		menuId = NAV__MENU_EDITION;

	ImGui::Spacing();
	if (ImGui::Button(" Camera", ImVec2(230 - 15, 39)))
		menuId = NAV__MENU_CAMERA;
}