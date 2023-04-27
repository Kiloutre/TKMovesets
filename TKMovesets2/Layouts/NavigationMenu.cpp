#include <ImGui.h>

#include "NavigationMenu.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "helpers.hpp"

bool DownloadProgramUpdate(s_updateStatus* updateStatus, GameAddressesFile* addresses, bool verify_only);

// - Const layout definition - //

const NavMenuBtn cg_moveset_btns[] = {
	{NAV__MENU_EXTRACT, "navmenu.extraction"},
	{NAV__MENU_IMPORT, "navmenu.import"},
};

const NavMenuBtn cg_play_btns[] = {
	{NAV__MENU_PERSISTENT_PLAY, "navmenu.persistent_play"},
	//{NAV__MENU_ONLINE_PLAY, "navmenu.online"},
};

const NavMenuBtn cg_tools_btns[] = {
	{NAV__MENU_EDITION, "navmenu.moveset_edit"},
	//{NAV__MENU_CAMERA, "navmenu.camera"},
};

const NavMenuBtn cg_other_btns[] = {
	//{NAV__MENU_DOCUMENTATION, "navmenu.documentation"},
	{NAV__MENU_ABOUT, "navmenu.about"},
};

// -- Private methods -- //

void NavigationMenu::RenderBtnList(const NavMenuBtn* c_btns, size_t count, float width, bool navigationLocked)
{
	for (size_t i = 0; i < count; ++i)
	{
		ImGui::Spacing();
		NavMenuBtn navBtn = c_btns[i];
		if (ImGuiExtra::RenderButtonEnabled(_(navBtn.c_name), menuId != navBtn.id && !navigationLocked, ImVec2(width, 30.0f))) {
			menuId = navBtn.id;
		}
	}
}


// -- Public methods -- //


NavigationMenu::NavigationMenu()
{
	m_languageId = Localization::GetCurrLangId();
	Localization::GetTranslationList(&m_translations, &m_translations_count);
}


NavigationMenu::~NavigationMenu()
{
	CleanupThread();
}


void NavigationMenu::Render(float width, bool navigationLocked)
{
	ImGui::SeparatorText(_("navmenu.category_moveset"));
	RenderBtnList(cg_moveset_btns, _countof(cg_moveset_btns), width, navigationLocked);

	ImGui::SeparatorText(_("navmenu.category_play"));
	RenderBtnList(cg_play_btns, _countof(cg_play_btns), width, navigationLocked);

	ImGui::SeparatorText(_("navmenu.category_tools"));
	RenderBtnList(cg_tools_btns, _countof(cg_tools_btns), width, navigationLocked);

	ImGui::SeparatorText(_("navmenu.category_other"));
	RenderBtnList(cg_other_btns, _countof(cg_other_btns), width, navigationLocked);

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
	ImGui::Separator();
	if (ImGuiExtra::RenderButtonEnabled(_("navmenu.update"), !m_updateStatus.verifying, ImVec2(width, 0))) {
		RequestCheckForUpdates();
	}

	if (m_updateStatus.verifying) {
		ImGui::TextUnformatted(_("navmenu.update_check"));
	}

	if (m_updateStatus.error) {
		ImGui::TextColored(ImVec4(1.0f, 0, 0, 1), _("navmenu.update_error"));
	}
	else {
		if (m_updateStatus.up_to_date) {
			ImGui::TextUnformatted(_("navmenu.up_to_date"));
		}
		else {
			if (m_updateStatus.addrFile) {
				ImGui::TextColored(ImVec4(0, 1.0f, 0, 1), _("navmenu.updated_addr"));
			}
		}

	}

	// Update popup
	if (m_updateStatus.programUpdateAvailable) {
		ImGui::OpenPopup("ProgramUpdatePopup");
		ImGui::SetNextWindowSizeConstraints(ImVec2(500, 200), ImVec2(1920, 1080));
	}
	if (ImGui::BeginPopupModal("ProgramUpdatePopup", &m_updateStatus.programUpdateAvailable))
	{
		ImGui::TextUnformatted(_("navmenu.update_explanation"));
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
			ImGuiExtra_TextboxError(_("navmenu.update_bad_file"));
		}

		ImGui::EndPopup();
	}
}