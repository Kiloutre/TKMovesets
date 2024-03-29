#pragma once

#include <ImGui.h>

#include "constants.h"

ImVec2 operator+(const ImVec2& c_self, const ImVec2& c_other);
ImVec2 operator-(const ImVec2& c_self, const ImVec2& c_other);
ImVec2 operator*(const ImVec2& c_self, float value);

namespace ImGuiExtra
{
	// Class that can be created in a block, will disable everything inside of that block
	class DisableBlockIf
	{
	private:
		bool m_disabled;

	public:
		DisableBlockIf(bool t_disabled)
		{
			m_disabled = t_disabled;
			if (m_disabled) {
				ImGui::BeginDisabled();
			}
		}

		~DisableBlockIf()
		{
			if (m_disabled) {
				ImGui::EndDisabled();
			}
		}
	};

	// Class that can be created in a block, will disable everything inside of that block
	class PushStyleColorBlockIf
	{
	private:
		bool m_disabled;

	public:
		template<typename T>
		PushStyleColorBlockIf(bool t_disabled, ImGuiCol idx, T col)
		{
			m_disabled = t_disabled;
			if (m_disabled) {
				ImGui::PushStyleColor(idx, col);
			}
		}

		~PushStyleColorBlockIf()
		{
			if (m_disabled) {
				ImGui::PopStyleColor();
			}
		}
	};

	// Renders markdown with proper wrapaorund
	void Markdown(const char* text, size_t text_size);
	void Markdown(const std::string& text);

	// Inits markdown callbacks and fonts
	void InitMarkdownConfig();

	// Displays a hoverable text that displays a tootip
	void HelpMarker(const char* desc, bool greyed=true);

	// Displays a tooltip if the last imgui item has been hovered
	void OnHoverTooltip(const char* text);

	// Render button that can be disabled (low alpha, always returns false)
	bool RenderButtonEnabled(const char* c_text, bool c_enabled, ImVec2 c_size=ImVec2(0, 0));

	// Renders a textbox with padding, border and filling color
	void RenderTextbox(const char* c_text, ImU32 c_borderColor=TEXTBOX_BORDER_COLOR, ImU32 c_fillColor=0, float padding=-1);
}

# define ImGuiExtra_TextboxWarning(text) ImGuiExtra::RenderTextbox(text, TEXTBOX_BORDER_WARNING, TEXTBOX_BORDER_WARNING)
# define ImGuiExtra_TextboxError(text) ImGuiExtra::RenderTextbox(text, TEXTBOX_BORDER_ERROR, TEXTBOX_BORDER_ERROR)
# define ImGuiExtra_TextboxTip(text) ImGuiExtra::RenderTextbox(text, TEXTBOX_BORDER_TIP, TEXTBOX_BORDER_TIP)
