#include <ImGui.h>
#include <format>

#include "EditorMove.hpp"
#include "Localization.hpp"

EditorMove::EditorMove(std::string windowTitleBase, uint16_t t_moveId)
{
	m_windowTitle = std::format("{} {} {}", windowTitleBase.c_str(), _("edition.window_title_move"), t_moveId);
	moveId = t_moveId;
}

void EditorMove::Render()
{
	if (setFocus) {
		ImGui::SetNextWindowFocus();
		setFocus = false;
	}

	if (ImGui::Begin(m_windowTitle.c_str(), &popen, unsavedChanges ? ImGuiWindowFlags_UnsavedDocument : 0))
	{
		ImGui::Text("testo");
	}
	ImGui::End();
	
	if (!popen && unsavedChanges) {
		// todo: show popup, force popen = true
	}
}