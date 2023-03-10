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
			// All these calcualtions are here to prevent tooltip from going outside of the current window's bound
			// With multiple viewports, going outside of bounds completely break the program

			// This serves as a safety margin: having a tooltip exactly the window's size or even slightly smaller still breaks things.
			/*
			const unsigned int margin = 20;

			ImGuiViewport* currentView = ImGui::GetWindowViewport();
			ImVec2 maxSize = currentView->Size;
			ImVec2 minBound = currentView->Pos;
			ImVec2 maxBound = minBound + maxSize;

			float textWrapPos = ImGui::GetFontSize() * 35.0f;
			ImVec2 windowSize = ImGui::CalcTextSize(desc, nullptr, false, textWrapPos);

			if ((windowSize.x + margin + 1) >= maxSize.x || (windowSize.y + margin + 1) >= maxSize.y) {
				// We could try different sizes here, try a couple times until we find the right on
				// todo, but low priority honestly
				return;
			}

			ImVec2 windowPos = ImGui::GetMousePos();

			if (windowPos.x + windowSize.x >= maxBound.x) {
				windowPos.x = maxBound.x - windowSize.x - margin;
			}

			if (windowPos.y + windowSize.y >= maxBound.y) {
				windowPos.y = maxBound.y - windowSize.y - margin;
			}
			*/

			//ImGui::SetNextWindowPos(windowPos);
			ImGui::BeginTooltip();

			float textWrapPos = ImGui::GetFontSize() * 35.0f;
			ImGui::PushTextWrapPos(textWrapPos);
			ImGui::TextUnformatted(desc);
			ImGui::PopTextWrapPos();

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

	void RenderTextbox(const char* c_text, ImU32 borderColor, ImU32 fillColor, float padding)
	{
		float HORIZ_PADDING = (padding < 0.0f ) ? TEXTBOX_HORIZ_PADDING : padding;
		float VERTICAL_PADDING = (padding < 0.0f) ? TEXTBOX_VERT_PADDING : padding;


		// Get starting drawing pos (useful for border)
		ImVec2 c_drawStart = ImGui::GetCursorPos();
		// Todo: correct start pos when scrolling. This actually does not work for some reason. Must test more.
		c_drawStart.y -= ImGui::GetScrollY();

		// Draw border & fill
		ImVec2 rectEnd;
		{
			{
				ImVec2 textSize = ImGui::CalcTextSize(c_text, nullptr, false, ImGui::GetContentRegionAvail().x - (HORIZ_PADDING * 2.0f));
				rectEnd = c_drawStart + textSize + (ImVec2(HORIZ_PADDING, VERTICAL_PADDING) * 2.0f);
			}

			ImDrawList* drawlist = ImGui::GetWindowDrawList();

			const ImVec2 c_winPos = ImGui::GetWindowPos();
			if (fillColor != 0) {
				drawlist->AddRectFilled(c_drawStart + c_winPos, rectEnd + c_winPos, (fillColor & 0x00FFFFFF) | (70 << 24));
			}
			drawlist->AddRect(c_drawStart + c_winPos, rectEnd + c_winPos, borderColor);
		}

		// Shift cursor position according to padding to not draw text at the top left of the box
		ImGui::SetCursorPos(c_drawStart + ImVec2(HORIZ_PADDING, VERTICAL_PADDING));

		// Draw text
		// I can't manage to reproduce the TextWrapped calculations so this works out for now
		ImGui::PushTextWrapPos(rectEnd.x - HORIZ_PADDING);
		ImGui::Text(c_text);
		ImGui::PopTextWrapPos();

		// Fix cursor position that is incorrect after drawing the text
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + VERTICAL_PADDING);
	}
}


