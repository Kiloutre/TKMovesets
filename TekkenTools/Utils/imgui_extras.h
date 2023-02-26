#pragma once
#include <ImGui.h>
#include "constants.h"

void ImGuiExtra_RenderTextbox(const char* text);
void ImGuiExtra_RenderTextbox(const char* text, ImU32 borderColor, ImU32 fillColor=0);

# define ImGuiExtra_TextboxWarning(text) ImGuiExtra_RenderTextbox(text, TEXTBOX_BORDER_WARNING, TEXTBOX_BORDER_WARNING)
# define ImGuiExtra_TextboxError(text) ImGuiExtra_RenderTextbox(text, TEXTBOX_BORDER_ERROR, TEXTBOX_BORDER_ERROR)
# define ImGuiExtra_TextboxTip(text) ImGuiExtra_RenderTextbox(text, TEXTBOX_BORDER_TIP, TEXTBOX_BORDER_TIP)