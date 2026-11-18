#include <cmath>
#include <algorithm>
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

void ImGuiExtra::ScrewIcon(ImVec2 min, float size, const char* tooltip)
{
	auto* draw = ImGui::GetWindowDrawList();
	const ImVec2 centre(min.x + size / 2, min.y + size / 2);
	const ImU32 ink = IM_COL32(255, 196, 70, 255);
	// Spiral: two and a half turns, from the centre outwards.
	constexpr int Points = 40;
	const float turns = 2.5f, radius = size * 0.42f;
	for (int i = 0; i <= Points; ++i) {
		const float t = float(i) / Points, a = t * turns * 6.2831853f;
		draw->PathLineTo(ImVec2(centre.x + std::cos(a) * radius * t, centre.y + std::sin(a) * radius * t));
	}
	draw->PathStroke(ink, 0, (std::max)(1.5f, size * 0.11f));
	if (tooltip && ImGui::IsMouseHoveringRect(min, ImVec2(min.x + size, min.y + size)) && ImGui::IsWindowHovered()) ImGui::SetTooltip("%s", tooltip);
}

namespace
{
	void IconTooltip(ImVec2 min, float size, const char* tooltip)
	{
		if (tooltip && ImGui::IsMouseHoveringRect(min, ImVec2(min.x + size, min.y + size)) && ImGui::IsWindowHovered()) ImGui::SetTooltip("%s", tooltip);
	}
}

void ImGuiExtra::RageIcon(ImVec2 min, float size, const char* tooltip)
{
	auto* draw = ImGui::GetWindowDrawList();
	auto at = [&](float x, float y) { return ImVec2(min.x + x * size, min.y + y * size); };
	// Flame: a pointed outer tongue, then a lighter inner one.
	const ImVec2 outer[] = { at(0.50f, 0.06f), at(0.68f, 0.30f), at(0.84f, 0.52f), at(0.80f, 0.78f), at(0.62f, 0.94f),
		at(0.38f, 0.94f), at(0.20f, 0.78f), at(0.16f, 0.54f), at(0.30f, 0.36f) };
	draw->AddConvexPolyFilled(outer + 1, 8, IM_COL32(225, 45, 35, 255)); // body (convex part)
	draw->AddTriangleFilled(outer[0], outer[1], outer[8], IM_COL32(225, 45, 35, 255)); // tip
	const ImVec2 inner[] = { at(0.52f, 0.46f), at(0.66f, 0.68f), at(0.60f, 0.86f), at(0.40f, 0.86f), at(0.34f, 0.68f) };
	draw->AddConvexPolyFilled(inner, 5, IM_COL32(255, 170, 60, 255));
	IconTooltip(min, size, tooltip);
}

void ImGuiExtra::HeatIcon(ImVec2 min, float size, const char* tooltip)
{
	auto* draw = ImGui::GetWindowDrawList();
	auto at = [&](float x, float y) { return ImVec2(min.x + x * size, min.y + y * size); };
	const ImU32 ink = IM_COL32(70, 170, 255, 255);
	// Lightning bolt: two triangles meeting at the middle.
	draw->AddTriangleFilled(at(0.62f, 0.04f), at(0.22f, 0.56f), at(0.56f, 0.52f), ink);
	draw->AddTriangleFilled(at(0.44f, 0.48f), at(0.78f, 0.44f), at(0.36f, 0.96f), ink);
	IconTooltip(min, size, tooltip);
}
