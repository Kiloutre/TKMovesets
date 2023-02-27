#pragma once
#include <ImGui.h>
#include "constants.h"

ImVec2 operator+(const ImVec2& self, const ImVec2& other);
ImVec2 operator*(const ImVec2& self, const float value);

namespace ImGuiExtra
{
	// Render button that can be disabled
	bool RenderButtonEnabled(const char* text, bool enabled, ImVec2 size=ImVec2(0, 0));

	// Textboxes with padding and border color/filling color
	void RenderTextbox(const char* text, ImU32 borderColor=TEXTBOX_BORDER_COLOR, ImU32 fillColor=0);
}

# define ImGuiExtra_TextboxWarning(text) ImGuiExtra::RenderTextbox(text, TEXTBOX_BORDER_WARNING, TEXTBOX_BORDER_WARNING)
# define ImGuiExtra_TextboxError(text) ImGuiExtra::RenderTextbox(text, TEXTBOX_BORDER_ERROR, TEXTBOX_BORDER_ERROR)
# define ImGuiExtra_TextboxTip(text) ImGuiExtra::RenderTextbox(text, TEXTBOX_BORDER_TIP, TEXTBOX_BORDER_TIP)
