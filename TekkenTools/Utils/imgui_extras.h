#pragma once
#include <ImGui.h>
#include "constants.h"

namespace ImGuiExtra
{
	// Render button that can be disabled
	bool RenderButtonEnabled(const char* text, bool enabled, ImVec2 size);

	// Textboxes with padding and border color/filling color
	void RenderTextbox(const char* text, ImU32 borderColor=TEXTBOX_BORDER_COLOR, ImU32 fillColor=0);
}

# define ImGuiExtra_TextboxWarning(text) ImGuiExtra::RenderTextbox(text, TEXTBOX_BORDER_WARNING, TEXTBOX_BORDER_WARNING)
# define ImGuiExtra_TextboxError(text) ImGuiExtra::RenderTextbox(text, TEXTBOX_BORDER_ERROR, TEXTBOX_BORDER_ERROR)
# define ImGuiExtra_TextboxTip(text) ImGuiExtra::RenderTextbox(text, TEXTBOX_BORDER_TIP, TEXTBOX_BORDER_TIP)
