#include <ImGui.h>

#include "Submenu_OnlinePlay.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"

void Submenu_OnlinePlay::Render()
{
	ImGuiExtra::RenderTextbox(_("online.explanation"));
	ImGui::SeparatorText(_("importation.select_moveset"));


}
