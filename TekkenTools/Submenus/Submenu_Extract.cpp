#include "Submenu_Extract.h"

Submenu_Extract* Submenu_Extract::s_instance = nullptr;

Submenu_Extract::Submenu_Extract()
{
	// Init
}

void RenderTextbox(const char* text, float horizontal_padding=10.0f, float vertical_padding=5.0f)
{
	// Get starting drawing pos (useful for border)
	ImVec2 pos = ImGui::GetCursorPos();

	// Shift cursor position according to padding to not draw text at the top left of the box
	ImGui::SetCursorPos(ImVec2(pos.x + horizontal_padding, pos.y + vertical_padding));

	ImGui::PushTextWrapPos(ImGui::GetWindowContentRegionMax().x - horizontal_padding);
	ImGui::Text(text);
	ImGui::PopTextWrapPos();

	// Draw border
	ImVec2 max = ImGui::GetItemRectMax();
	max.x += horizontal_padding;
	max.y += vertical_padding;
	ImGui::GetWindowDrawList()->AddRect(pos, max, IM_COL32(255, 255, 0, 255));

	// Fix cursor position
	ImGui::SetCursorPosY(max.y);
}

void Submenu_Extract::Render() 
{
	const char* text = "Movesets have to first be extracted from the game into files on your computer to be then used. Extracts only movesets : nothing visual.\nExtraction is done with the game running (from memory), with the target character(s) loaded and visible on the screen (it doesn't matter what gamemode).";
	//ImGui::TextWrapped(text);
	RenderTextbox(text);
	ImGui::Separator();
	ImGui::TextWrapped(text);
}
