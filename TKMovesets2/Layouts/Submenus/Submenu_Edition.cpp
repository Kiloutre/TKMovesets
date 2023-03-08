#include <ImGui.h>

#include "Submenu_Edition.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"

void Submenu_Edition::Render()
{
	ImGuiExtra::RenderTextbox(_("edition.explanation"));
	ImGui::SeparatorText(_("edition.select_moveset"));
}
