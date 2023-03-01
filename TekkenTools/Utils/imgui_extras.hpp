#pragma once
#include <ImGui.h>

#include "constants.h"

ImVec2 operator+(const ImVec2& c_self, const ImVec2& c_other);
ImVec2 operator*(const ImVec2& c_self, float value);

namespace ImGuiExtra
{
	// Displays a hoverable text that displays a tootip
	void HelpMarker(const char* desc);

	// Render button that can be disabled (low alpha, always returns false)
	bool RenderButtonEnabled(const char* c_text, bool c_enabled, ImVec2 c_size=ImVec2(0, 0));

	// Renders a textbox with padding, border and filling color
	void RenderTextbox(const char* c_text, ImU32 c_borderColor=TEXTBOX_BORDER_COLOR, ImU32 c_fillColor=0);
}

# define ImGuiExtra_TextboxWarning(text) ImGuiExtra::RenderTextbox(text, TEXTBOX_BORDER_WARNING, TEXTBOX_BORDER_WARNING)
# define ImGuiExtra_TextboxError(text) ImGuiExtra::RenderTextbox(text, TEXTBOX_BORDER_ERROR, TEXTBOX_BORDER_ERROR)
# define ImGuiExtra_TextboxTip(text) ImGuiExtra::RenderTextbox(text, TEXTBOX_BORDER_TIP, TEXTBOX_BORDER_TIP)
