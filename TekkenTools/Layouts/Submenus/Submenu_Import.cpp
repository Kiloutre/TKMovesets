#include <ImGui.h>

#include "Submenu_Import.h"
#include "Localization.h"
#include "imgui_extras.h"

Submenu_Import::Submenu_Import()
{
	// Init
}

void Submenu_Import::Render()
{
	ImGuiExtra_RenderTextbox(_("importation.explanation"));
	ImGui::TextWrapped(_("importation.select_moveset"));
	ImGui::SeparatorText(_("importation.game_not_running"));
	ImGuiExtra_RenderTextbox(_("importation.game_not_running"), TEXTBOX_BORDER_WARNING);
}
