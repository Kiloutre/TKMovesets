#include "imgui_extras.h"
#include "constants.h"

ImVec2 operator+(const ImVec2& self, const ImVec2& other)
{
	return ImVec2(self.x + other.x, self.y + other.y);
}
ImVec2 operator*(const ImVec2& self, const float value)
{
	return ImVec2(self.x * value, self.y * value);
}

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
	
	void RenderTextbox2(const char* text, ImU32 borderColor, ImU32 fillColor, ImVec2 padding, ImVec2 minSize, ImVec2 maxSize)
	{
		const ImVec2 drawStart = ImGui::GetCursorPos();

		// Draw border & fill
		ImVec2 rectEnd;
		{
			{
				ImVec2 textSize = ImGui::CalcTextSize(text, NULL, false, ImGui::GetContentRegionAvail().x - (TEXTBOX_HORIZ_PADDING * 2.0f));
				rectEnd = drawStart + textSize + (ImVec2(TEXTBOX_HORIZ_PADDING, TEXTBOX_VERT_PADDING) * 2.0f);
			}

			ImDrawList* drawlist = ImGui::GetWindowDrawList();

			ImVec2 winPos = ImGui::GetWindowPos();
			if (fillColor != 0) drawlist->AddRectFilled(drawStart + winPos, rectEnd + winPos, (fillColor & 0x00FFFFFF) | (75 << 24));
			drawlist->AddRect(drawStart + winPos, rectEnd + winPos, borderColor);
		}

		// Shift cursor position according to padding to not draw text at the top left of the box
		ImGui::SetCursorPos(drawStart + ImVec2(TEXTBOX_HORIZ_PADDING, TEXTBOX_VERT_PADDING));

		// Draw text
		// I can't manage to reproduce the TextWrapped calculations so this works out for now
		ImGui::PushTextWrapPos(rectEnd.x - TEXTBOX_HORIZ_PADDING);
		ImGui::Text(text);
		ImGui::PopTextWrapPos();

		// Fix cursor position that is incorrect after drawing the text
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + TEXTBOX_VERT_PADDING);
	}
	

	void RenderTextbox(const char* text, ImU32 borderColor, ImU32 fillColor)
	{
		// Get starting drawing pos (useful for border)
		const ImVec2 drawStart = ImGui::GetCursorPos();

		// Draw border & fill
		ImVec2 rectEnd;
		{
			{
				ImVec2 textSize = ImGui::CalcTextSize(text, NULL, false, ImGui::GetContentRegionAvail().x - (TEXTBOX_HORIZ_PADDING * 2.0f));
				rectEnd = drawStart + textSize + (ImVec2(TEXTBOX_HORIZ_PADDING, TEXTBOX_VERT_PADDING) * 2.0f);
			}

			ImDrawList* drawlist = ImGui::GetWindowDrawList();

			ImVec2 winPos = ImGui::GetWindowPos();
			if (fillColor != 0) drawlist->AddRectFilled(drawStart + winPos, rectEnd + winPos, (fillColor & 0x00FFFFFF) | (75 << 24));
			drawlist->AddRect(drawStart + winPos, rectEnd + winPos, borderColor);
		}

		// Shift cursor position according to padding to not draw text at the top left of the box
		ImGui::SetCursorPos(drawStart + ImVec2(TEXTBOX_HORIZ_PADDING, TEXTBOX_VERT_PADDING));

		// Draw text
		// I can't manage to reproduce the TextWrapped calculations so this works out for now
		ImGui::PushTextWrapPos(rectEnd.x - TEXTBOX_HORIZ_PADDING);
		ImGui::Text(text);
		ImGui::PopTextWrapPos();

		// Fix cursor position that is incorrect after drawing the text
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + TEXTBOX_VERT_PADDING);
	}
}


