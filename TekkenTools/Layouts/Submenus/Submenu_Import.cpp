#include "Submenu_Import.h"
#include "imgui_extras.h"

Submenu_Import* Submenu_Import::s_instance = nullptr;

Submenu_Import::Submenu_Import()
{
	// Init
}

void Submenu_Import::Render()
{
	ImGuiExtra_RenderTextbox(_("importation.explanation"));
	ImGui::SeparatorText(_("importation.select_moveset"));
	ImGuiExtra_RenderTextbox(_("importation.game_not_running"), TEXTBOX_BORDER_WARNING);
}
