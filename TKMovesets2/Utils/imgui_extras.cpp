#include "imgui.h"
#include <imgui_markdown.h>       // https://github.com/juliettef/imgui_markdown
#include <string>
#include <algorithm>

#include "imgui_extras.hpp"

#include "constants.h"

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
    void RenderTextbox(const char* text, ImU32 borderColor, ImU32 fillColor, float padding)
    {
        const float horizontal = padding < 0 ? TEXTBOX_HORIZ_PADDING : padding;
        const float vertical = padding < 0 ? TEXTBOX_VERT_PADDING : padding;
        const float available = ImGui::GetContentRegionAvail().x - 2 * horizontal;
        const float wrapWidth = available > 1 ? available : 1;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 textSize = ImGui::CalcTextSize(text, nullptr, false, wrapWidth);
        const ImVec2 size = textSize + ImVec2(2 * horizontal, 2 * vertical);
        auto draw = ImGui::GetWindowDrawList();
        if (fillColor)
            draw->AddRectFilled(origin, origin + size, (fillColor & 0x00FFFFFF) | (70 << 24));
        draw->AddRect(origin, origin + size, borderColor);
        ImGui::SetCursorScreenPos(origin + ImVec2(horizontal, vertical));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapWidth);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        // Cursor movement alone does not reserve layout space, including bottom padding.
        ImGui::SetCursorScreenPos(origin);
        ImGui::Dummy(size);
    }

    bool BeginFittedModal(const char* id, const char* title, bool* open, float widthEm)
    {
        // "title###id": the visible title can be localized while OpenPopup keeps using "###id".
        const std::string label = std::string(title) + "###" + id;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        // Opens a quarter wider than asked and at least 46 em, capped to the screen (callers' widths were tuned for a
        // fixed modal and left long lines cramped).
        const float width = (std::min)(ImGui::GetFontSize() * (std::max)(widthEm * 1.25f, 46.0f), viewport->WorkSize.x * 0.9f);
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        // Opens at `width` with its height fitted to the content once (a zero height auto-fits that axis on the first
        // frame), then stays resizable: wrapped text follows the new width, and content that does not wrap (long
        // labels, tables, paths) scrolls instead of running past the border.
        ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Appearing);
        // The height fits the content when it opens, but not below about 14 lines (half the screen at most).
        ImGui::SetNextWindowSizeConstraints(ImVec2((std::min)(ImGui::GetFontSize() * 16.0f, width),
            (std::min)(ImGui::GetFontSize() * 14.0f, viewport->WorkSize.y * 0.5f)),
            ImVec2(viewport->WorkSize.x * 0.95f, viewport->WorkSize.y * 0.95f));
        return ImGui::BeginPopupModal(label.c_str(), open, ImGuiWindowFlags_HorizontalScrollbar);
    }
}
