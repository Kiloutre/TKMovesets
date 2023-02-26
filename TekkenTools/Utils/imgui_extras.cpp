#include "imgui_extras.h"

// This is a mess, does not work properly  and needs to be re-done entirely, to be honest.

void ImGuiExtra_RenderTextbox(const char* text)
{
	ImGuiExtra_RenderTextbox(text, TEXTBOX_BORDER_COLOR);
}

void ImGuiExtra_RenderTextbox(const char* text, ImU32 borderColor, ImU32 fillColor)
{
	// Get starting drawing pos (useful for border)
	const ImVec2 drawStart = ImGui::GetCursorPos();

	// Draw border
	{
		ImVec2 textSize = ImGui::CalcTextSize(text, NULL, false, ImGui::GetContentRegionAvail().x - TEXTBOX_HORIZ_PADDING);
		ImVec2 rectEnd = ImVec2(drawStart.x + textSize.x, drawStart.y + textSize.y + (TEXTBOX_VERT_PADDING * 2.0f));

		ImDrawList* drawlist = ImGui::GetWindowDrawList();

		if (fillColor != 0) drawlist->AddRectFilled(drawStart, rectEnd, (fillColor & 0x00FFFFFF) | (75 << 24));
		drawlist->AddRect(drawStart, rectEnd, borderColor);
	}

	// Shift cursor position according to padding to not draw text at the top left of the box
	ImGui::SetCursorPos(ImVec2(drawStart.x + (TEXTBOX_HORIZ_PADDING * 1.5f), drawStart.y + TEXTBOX_VERT_PADDING));

	// Draw text
	// I can't manage to reproduce the TextWrapped calculations so this works out for now
	ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x - (TEXTBOX_HORIZ_PADDING * 4));
	ImGui::Text(text);
	ImGui::PopTextWrapPos();

	// Fix cursor position that is incorrect after drawing the text
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + TEXTBOX_VERT_PADDING);
}

bool ImGUIExtra_RenderButton(const char* text, bool enabled, ImVec2 size)
{
	if (!enabled) return ImGui::Button(text, size);

	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	ImGui::Button(text, size);
	ImGui::PopStyleVar();
	return false;
}
