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
	ImGui::SeparatorText(_("importation.select_moveset"));
	ImGuiExtra_TextboxWarning(_("importation.game_not_running"));
}
