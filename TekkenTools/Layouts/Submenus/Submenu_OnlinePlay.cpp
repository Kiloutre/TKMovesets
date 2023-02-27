#include <ImGui.h>

#include "Submenu_OnlinePlay.h"
#include "Localization.h"
#include "imgui_extras.h"

Submenu_OnlinePlay::Submenu_OnlinePlay()
{
	// Init
}
void Submenu_OnlinePlay::Render()
{
	ImGuiExtra::RenderTextbox(_("online.explanation"));
	ImGui::SeparatorText(_("importation.select_moveset"));
}
