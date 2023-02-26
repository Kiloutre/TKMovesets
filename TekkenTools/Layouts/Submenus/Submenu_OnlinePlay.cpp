#include "Submenu_OnlinePlay.h"
#include "imgui_extras.h"

Submenu_OnlinePlay* Submenu_OnlinePlay::s_instance = nullptr;

Submenu_OnlinePlay::Submenu_OnlinePlay()
{
	// Init
}
void Submenu_OnlinePlay::Render()
{
	ImGuiExtra_RenderTextbox(_("online.explanation"));
	ImGui::SeparatorText(_("importation.select_moveset"));
	ImGuiExtra_RenderTextbox(_("importation.game_not_running"), TEXTBOX_BORDER_WARNING);
}
