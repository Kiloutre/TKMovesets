#include <ImGui.h>
#include "GLFW/glfw3.h"

#include "SideMenu.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "helpers.hpp"

bool DownloadProgramUpdate(s_updateStatus* updateStatus, GameAddressesFile* addresses, bool verify_only);


// -- Public methods -- //


SideMenu::SideMenu()
{
	m_languageId = Localization::GetCurrLangId();
	Localization::GetTranslationList(&m_translations, &m_translations_count);
}


SideMenu::~SideMenu()
{
	CleanupThread();
}


void SideMenu::Render(float width)
{
	// Language list
	ImGui::PushItemWidth(width);
	if (ImGui::BeginCombo("##", m_translations[m_languageId].displayName))
	{
		for (unsigned int i = 0; i < m_translations_count; ++i)
		{
			ImGui::PushID(&i);
			if (ImGui::Selectable(m_translations[i].displayName, i == m_languageId)) {
				if (Localization::LoadFile(m_translations[i].locale, true)) {
					m_languageId = Localization::GetCurrLangId();
				}
			}
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}
	ImGui::PopItemWidth();

	// Updating
	ImGui::NewLine();
	if (ImGuiExtra::RenderButtonEnabled(_("sidemenu.update"), !m_updateStatus.verifying, ImVec2(width, 0))) {
		RequestCheckForUpdates();
	}

	if (m_updateStatus.verifying) {
		ImGui::TextUnformatted(_("sidemenu.update_check"));
	}

	if (m_updateStatus.error) {
		ImGui::TextColored(ImVec4(1.0f, 0, 0, 1), _("sidemenu.update_error"));
	}
	else {
		if (m_updateStatus.up_to_date) {
			ImGui::TextUnformatted(_("sidemenu.up_to_date"));
		}
		else {
			if (m_updateStatus.addrFile) {
				ImGui::TextColored(ImVec4(0, 1.0f, 0, 1), _("sidemenu.updated_addr"));
			}
		}

	}

	ImGui::TextUnformatted(_("sidemenu.vsync"));
	char buf[2] = { '0' + m_vsync_setting , 0 };
	ImGui::PushID(&m_vsync_setting);
	if (ImGui::BeginCombo("##", buf))
	{
		for (unsigned int i = 0; i < 5; ++i)
		{
			ImGui::PushID(i);
			buf[0] = '0' + i;
			if (ImGui::Selectable(buf, i == m_vsync_setting)) {
				m_vsync_setting = i;
				glfwSwapInterval(i);
			}
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}
	ImGui::PopID();

	// Update popup
	if (m_updateStatus.programUpdateAvailable) {
		ImGui::OpenPopup("ProgramUpdatePopup");
		ImGui::SetNextWindowSizeConstraints(ImVec2(500, 200), ImVec2(1920, 1080));
	}
	if (ImGui::BeginPopupModal("ProgramUpdatePopup", &m_updateStatus.programUpdateAvailable))
	{
		ImGui::TextUnformatted(_("sidemenu.update_explanation"));
		ImGui::SeparatorText(m_updateStatus.tagNameSeparatorText.c_str());

		if (ImGui::BeginTable("table_row_height", 1, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV))
		{
			for (auto& line : m_updateStatus.changelog)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(line.c_str());
			}
			ImGui::EndTable();
		}

		if (ImGui::Button(_("yes"))) {
			if (DownloadProgramUpdate(&m_updateStatus, m_addresses, false)) {
				*requestedUpdatePtr = true;
			}
			else {
				m_updateFileInvalid = true;
			}
		}
        ImGui::SameLine();
		if (ImGui::Button(_("no"))) {
			ImGui::CloseCurrentPopup();
			m_updateStatus.programUpdateAvailable = false;
		}

		if (m_updateFileInvalid) {
			ImGuiExtra_TextboxError(_("sidemenu.update_bad_file"));
		}

		ImGui::EndPopup();
	}
}