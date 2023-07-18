#include "imgui.h"
#include <imgui_markdown.h>       // https://github.com/juliettef/imgui_markdown
#include <string>

#include "imgui_extras.hpp"

#include "constants.h"

ImVec2 operator+(const ImVec2& c_self, const ImVec2& c_other)
{
	return ImVec2(c_self.x + c_other.x, c_self.y + c_other.y);
}
ImVec2 operator-(const ImVec2& c_self, const ImVec2& c_other)
{
	return ImVec2(c_self.x - c_other.x, c_self.y - c_other.y);
}
ImVec2 operator*(const ImVec2& c_self, float value)
{
	return ImVec2(c_self.x * value, c_self.y * value);
}

static struct {
	ImGui::MarkdownConfig config;
	ImFont* H1 = NULL;
	ImFont* H2 = NULL;
	ImFont* H3 = NULL;
} s_markdownConfig;

namespace ImGuiExtra
{
	void Markdown(const std::string& text)
	{
		Markdown(text.c_str(), text.size());
	}

	void Markdown(const char* text, size_t text_size)
	{
		ImGui::Markdown(text, text_size, s_markdownConfig.config);
	}

	void InitMarkdownConfig()
	{
		auto& io = ImGui::GetIO();
		s_markdownConfig.H1 = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/arial.ttf", 23);
		s_markdownConfig.H2 = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/arial.ttf", 18);
		s_markdownConfig.config.headingFormats[0] = { s_markdownConfig.H1, true };
		s_markdownConfig.config.headingFormats[1] = { s_markdownConfig.H2, true };
		//s_markdownConfig.config.headingFormats[2] = { s_markdownConfig.H3, false };
	}

	void HelpMarker(const char* desc, bool greyed)
	{
		if (greyed) {
			ImGui::TextDisabled("(?)");
		}
		else {
			ImGui::TextColored(ImVec4(1, 0, 0.2f, 1), "(?)");
		}

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
		{
			ImGui::BeginTooltip();

			float textWrapPos = ImGui::GetFontSize() * 35.0f;
			ImGui::PushTextWrapPos(textWrapPos);
			ImGui::TextUnformatted(desc);
			ImGui::PopTextWrapPos();

			ImGui::EndTooltip();
		}
	}

	void OnHoverTooltip(const char* text)
	{
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
		{
			ImGui::BeginTooltip();
			ImGui::NewLine();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(text);
			ImGui::PopTextWrapPos();
			ImGui::NewLine();
			ImGui::EndTooltip();
		}
	}

	bool RenderButtonEnabled(const char* c_text, bool enabled, ImVec2 size)
	{
		if (enabled) {
			return ImGui::Button(c_text, size);
		}
		ImGui::BeginDisabled();
		ImGui::Button(c_text, size);
		ImGui::EndDisabled();
		return false;
	}

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


