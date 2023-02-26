#include "imgui_extras.h"

// This is a mess, does not work properly  and needs to be re-done entirely, to be honest.
void ImGuiExtra_RenderTextbox(const char* text, float horizontal_padding, float vertical_padding)
{
	// Get starting drawing pos (useful for border)
	const ImVec2 drawStart = ImGui::GetCursorPos();

	// Draw border
	{
		ImVec2 textSize = ImGui::CalcTextSize(text, NULL, false, ImGui::GetContentRegionAvail().x - horizontal_padding);
		ImGui::GetWindowDrawList()->AddRect(drawStart, ImVec2(drawStart.x + textSize.x, drawStart.y + textSize.y + (vertical_padding * 2.0f)), IM_COL32(255, 255, 0, 255));
	}

	// Shift cursor position according to padding to not draw text at the top left of the box
	ImGui::SetCursorPos(ImVec2(drawStart.x + (horizontal_padding * 1.5f), drawStart.y + vertical_padding));

	// Draw text
	// I can't manage to reproduce the TextWrapped calculations so this works out for now
	ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x - (horizontal_padding * 4));
	ImGui::Text(text);
	ImGui::PopTextWrapPos();

	// Fix cursor position that is incorrect after drawing the text
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + vertical_padding);
}