#include "imgui_extras.hpp"
#include "constants.h"

ImVec2 operator+(const ImVec2& c_self, const ImVec2& c_other)
{
	return ImVec2(c_self.x + c_other.x, c_self.y + c_other.y);
}
ImVec2 operator*(const ImVec2& c_self, float value)
{
	return ImVec2(c_self.x * value, c_self.y * value);
}

namespace ImGuiExtra
{
	void HelpMarker(const char* desc)
	{
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
		{
			ImGui::BeginTooltip();
			//ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(desc);
			//ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	bool RenderButtonEnabled(const char* c_text, bool enabled, ImVec2 size)
	{
		if (enabled) {
			return ImGui::Button(c_text, size);
		}

		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

		ImGui::Button(c_text, size);

		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		return false;
	}

	// This is a mess, does not work properly  and needs to be re-done entirely, to be honest.
	// Todo: min / max size, allow centering of text
	
	void RenderTextbox2(const char* c_text, ImU32 borderColor, ImU32 fillColor, ImVec2 padding, ImVec2 minSize, ImVec2 maxSize)
	{
		// Todo
	}
	

	void RenderTextbox(const char* c_text, ImU32 borderColor, ImU32 fillColor)
	{
		// Get starting drawing pos (useful for border)
		ImVec2 c_drawStart = ImGui::GetCursorPos();
		// Todo: correct start pos when scrolling. This actually does not work for some reason. Must test more.
		c_drawStart.y -= ImGui::GetScrollY();

		// Draw border & fill
		ImVec2 rectEnd;
		{
			{
				ImVec2 textSize = ImGui::CalcTextSize(c_text, nullptr, false, ImGui::GetContentRegionAvail().x - (TEXTBOX_HORIZ_PADDING * 2.0f));
				rectEnd = c_drawStart + textSize + (ImVec2(TEXTBOX_HORIZ_PADDING, TEXTBOX_VERT_PADDING) * 2.0f);
			}

			ImDrawList* drawlist = ImGui::GetWindowDrawList();

			const ImVec2 c_winPos = ImGui::GetWindowPos();
			if (fillColor != 0) {
				drawlist->AddRectFilled(c_drawStart + c_winPos, rectEnd + c_winPos, (fillColor & 0x00FFFFFF) | (70 << 24));
			}
			drawlist->AddRect(c_drawStart + c_winPos, rectEnd + c_winPos, borderColor);
		}

		// Shift cursor position according to padding to not draw text at the top left of the box
		ImGui::SetCursorPos(c_drawStart + ImVec2(TEXTBOX_HORIZ_PADDING, TEXTBOX_VERT_PADDING));

		// Draw text
		// I can't manage to reproduce the TextWrapped calculations so this works out for now
		ImGui::PushTextWrapPos(rectEnd.x - TEXTBOX_HORIZ_PADDING);
		ImGui::Text(c_text);
		ImGui::PopTextWrapPos();

		// Fix cursor position that is incorrect after drawing the text
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + TEXTBOX_VERT_PADDING);
	}
}


