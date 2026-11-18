#pragma once

#include <ImGui.h>

#include "constants.h"

namespace ImGuiExtra
{
	// Screw (tornado) icon of the move lists: a spiral in a square of side `size` at `min`, drawn over the last item.
	// Shows `tooltip` when the pointer is over it.
	void ScrewIcon(ImVec2 min, float size, const char* tooltip);
	// Rage icon of the move lists (the move spends rage): a red flame. Same placement and tooltip as ScrewIcon.
	void RageIcon(ImVec2 min, float size, const char* tooltip);
	// Heat icon of the move lists (the move spends heat): a blue lightning bolt. Same placement and tooltip as ScrewIcon.
	void HeatIcon(ImVec2 min, float size, const char* tooltip);
	// Left edge of the [slot]th move-list icon (0 = rightmost) in a name cell at [cellX], [cellWidth] wide.
	inline float MoveIconX(float cellX, float cellWidth, float size, int slot) { return cellX + cellWidth - (size + 2) * float(slot + 1); }
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

	// Modal used by every dialog: opens centered at 1.25 x `widthEm` font sizes (at least 46, capped to the screen)
	// with its height fitted to the content (at least ~14 lines), then resizable; content wider than the window
	// scrolls. Shown with `title`.
	// `id` is the identifier passed to ImGui::OpenPopup.
	bool BeginFittedModal(const char* id, const char* title, bool* open = nullptr, float widthEm = 34.0f);
}

# define ImGuiExtra_TextboxWarning(text) ImGuiExtra::RenderTextbox(text, TEXTBOX_BORDER_WARNING, TEXTBOX_BORDER_WARNING)
# define ImGuiExtra_TextboxError(text) ImGuiExtra::RenderTextbox(text, TEXTBOX_BORDER_ERROR, TEXTBOX_BORDER_ERROR)
# define ImGuiExtra_TextboxTip(text) ImGuiExtra::RenderTextbox(text, TEXTBOX_BORDER_TIP, TEXTBOX_BORDER_TIP)
