#include "imgui_extras.h"
#include "constants.h"

namespace ImGuiExtra
{
	bool RenderButtonEnabled(const char* text, bool enabled, ImVec2 size)
	{
		if (!enabled) return ImGui::Button(text, size);

		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.8f);

		ImGui::Button(text, size);

		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		return false;
	}

	void RenderTextbox(const char* text)
	{
		ImGuiExtra::RenderTextbox(text, TEXTBOX_BORDER_COLOR);
	}

	// This is a mess, does not work properly  and needs to be re-done entirely, to be honest.
	// Todo: min / max size, allow centering of text
	void RenderTextbox(const char* text, ImU32 borderColor, ImU32 fillColor)
	{
		// Get starting drawing pos (useful for border)
		const ImVec2 drawStart = ImGui::GetCursorPos();

		// Draw border
		float rectEndX;
		{
			ImVec2 textSize = ImGui::CalcTextSize(text, NULL, false, ImGui::GetContentRegionAvail().x - (TEXTBOX_HORIZ_PADDING * 2.0f));
			ImVec2 rectEnd = ImVec2(drawStart.x + textSize.x + (TEXTBOX_HORIZ_PADDING * 2.0f), drawStart.y + textSize.y + (TEXTBOX_VERT_PADDING * 2.0f));
			rectEndX = rectEnd.x;

			ImDrawList* drawlist = ImGui::GetWindowDrawList();

			if (fillColor != 0) drawlist->AddRectFilled(drawStart, rectEnd, (fillColor & 0x00FFFFFF) | (75 << 24));
			drawlist->AddRect(drawStart, rectEnd, borderColor);
		}

		// Shift cursor position according to padding to not draw text at the top left of the box
		ImGui::SetCursorPos(ImVec2(drawStart.x + TEXTBOX_HORIZ_PADDING, drawStart.y + TEXTBOX_VERT_PADDING));

		// Draw text
		// I can't manage to reproduce the TextWrapped calculations so this works out for now
		ImGui::PushTextWrapPos(rectEndX - TEXTBOX_HORIZ_PADDING);
		ImGui::Text(text);
		ImGui::PopTextWrapPos();

		// Fix cursor position that is incorrect after drawing the text
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + TEXTBOX_VERT_PADDING);
	}
}


