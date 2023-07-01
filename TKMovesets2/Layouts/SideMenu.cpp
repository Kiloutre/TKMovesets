#include <ImGui.h>
#include "GLFW/glfw3.h"
#include "shlobj_core.h"

#include "Settings.hpp"
#include "SideMenu.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "helpers.hpp"

bool DownloadProgramUpdate(s_updateStatus* updateStatus, GameAddressesFile* addresses, bool verify_only);

// -- Static helpers -- //

// Detects keypresses for setting keybind 
static void DetectKeypresses(s_Keybind& out)
{
	for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k)
	{
		ImGuiKey _k = (ImGuiKey)k;
		if (ImGui::IsKeyPressed(_k, false) && !out.contains(_k) && !Keybinds::IsForbiddenKey(_k))
		{
			// Found new key
			out.insert(_k);
		}
	}
}

//Sets UTF-8 registry value
static void RegSet(HKEY hkeyHive, const char* pszVar, const char* pszValue)
{
	HKEY hkey;

	char szValueCurrent[1000];
	DWORD dwType;
	DWORD dwSize = sizeof(szValueCurrent);

	int iRC = RegGetValue(hkeyHive, pszVar, NULL, RRF_RT_ANY, &dwType, szValueCurrent, &dwSize);

	bool bDidntExist = iRC == ERROR_FILE_NOT_FOUND;

	if (iRC != ERROR_SUCCESS && !bDidntExist) {
		char buf[128];
		strerror_s(buf, sizeof(buf), iRC);
		DEBUG_ERR("RegGetValue( %s ): %s", pszVar, buf);
	}

	if (!bDidntExist) {
		if (dwType != REG_SZ) {
			DEBUG_ERR("RegGetValue( %s ) found type unhandled %d", pszVar, dwType);
		}

		if (strcmp(szValueCurrent, pszValue) == 0) {
			DEBUG_ERR("RegSet( \"%s\" \"%s\" ): already correct", pszVar, pszValue);
			return;
		}
	}

	DWORD dwDisposition;
	iRC = RegCreateKeyEx(hkeyHive, pszVar, 0, 0, 0, KEY_ALL_ACCESS, NULL, &hkey, &dwDisposition);
	if (iRC != ERROR_SUCCESS) {
		char buf[128];
		strerror_s(buf, sizeof(buf), iRC);
		DEBUG_ERR("RegCreateKeyEx( %s ): %s", pszVar, buf);
	}

	iRC = RegSetValueEx(hkey, "", 0, REG_SZ, (BYTE*)pszValue, (DWORD)strlen(pszValue) + 1);
	if (iRC != ERROR_SUCCESS) {
		char buf[128];
		strerror_s(buf, sizeof(buf), iRC);
		DEBUG_ERR("RegSetValueEx( %s ): %s", pszVar, buf);
	}

	if (bDidntExist)
		DEBUG_ERR("RegSet( %s ): set to \"%s\"", pszVar, pszValue);
	else
		DEBUG_ERR("RegSet( %s ): changed \"%s\" to \"%s\"", pszVar, szValueCurrent, pszValue);

	RegCloseKey(hkey);
}

//Sets UTF-32 registry value
static void RegSetW(HKEY hkeyHive, const wchar_t* pszVar, const wchar_t* pszValue)
{
	HKEY hkey;

	wchar_t szValueCurrent[1000];
	DWORD dwType;
	DWORD dwSize = sizeof(szValueCurrent);

	int iRC = RegGetValueW(hkeyHive, pszVar, NULL, RRF_RT_ANY, &dwType, szValueCurrent, &dwSize);

	bool bDidntExist = iRC == ERROR_FILE_NOT_FOUND;

	if (iRC != ERROR_SUCCESS && !bDidntExist) {
		char buf[128];
		strerror_s(buf, sizeof(buf), iRC);
		DEBUG_ERR("RegGetValue( %S ): %s", pszVar, buf);
	}

	if (!bDidntExist) {
		if (dwType != REG_SZ) {
			DEBUG_ERR("RegGetValue( %S ) found type unhandled %d", pszVar, dwType);
		}

		if (wcscmp(szValueCurrent, pszValue) == 0) {
			DEBUG_ERR("RegSet( \"%S\" \"%S\" ): already correct", pszVar, pszValue);
			return;
		}
	}

	DWORD dwDisposition;
	iRC = RegCreateKeyExW(hkeyHive, pszVar, 0, 0, 0, KEY_ALL_ACCESS, NULL, &hkey, &dwDisposition);
	if (iRC != ERROR_SUCCESS) {
		char buf[128];
		strerror_s(buf, sizeof(buf), iRC);
		DEBUG_ERR("RegCreateKeyEx( %S ): %s", pszVar, buf);
	}

	iRC = RegSetValueExW(hkey, L"", 0, REG_SZ, (BYTE*)pszValue, (DWORD)(wcslen(pszValue) + 1) * (DWORD)sizeof(wchar_t));
	if (iRC != ERROR_SUCCESS) {
		char buf[128];
		strerror_s(buf, sizeof(buf), iRC);
		DEBUG_ERR("RegSetValueEx( %S ): %s", pszVar, buf);
	}

	if (bDidntExist)
		DEBUG_ERR("RegSet( %S ): set to \"%S\"", pszVar, pszValue);
	else
		DEBUG_ERR("RegSet( %S ): changed \"%S\" to \"%S\"", pszVar, szValueCurrent, pszValue);

	RegCloseKey(hkey);
}

static void SetWindowsFileAssociation()
{
	wchar_t program_path[MAX_PATH] = { 0 };
	GetModuleFileNameW(nullptr, program_path, MAX_PATH);
	std::wstring moveset_command = L"\"" + std::wstring(program_path) + L"\" --move-moveset \"%1\"";
	std::wstring animation_command = L"\"" + std::wstring(program_path) + L"\" --move-animation \"%1\"";

	RegSet(HKEY_CURRENT_USER, "Software\\Classes\\" MOVESET_FILENAME_EXTENSION, "tkmvst_file");
	RegSet(HKEY_CURRENT_USER, "Software\\Classes\\" MOVESET_TMPFILENAME_EXTENSION, "tkmvst_file");
	RegSet(HKEY_CURRENT_USER, "Software\\Classes\\.tkanim64", "tkanim_file");
	RegSet(HKEY_CURRENT_USER, "Software\\Classes\\.tkanimC8", "tkanim_file");

	//RegSetW(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\TKMovesets2\\shell\\open\\command", command.c_str());
	RegSetW(HKEY_CURRENT_USER, L"Software\\Classes\\tkmvst_file\\shell\\open\\command", moveset_command.c_str());
	RegSetW(HKEY_CURRENT_USER, L"Software\\Classes\\tkanim_file\\shell\\open\\command", animation_command.c_str());
	RegSet(HKEY_CURRENT_USER, "Software\\Classes\\tkmvst_file", "TKM Moveset");
	RegSet(HKEY_CURRENT_USER, "Software\\Classes\\tkanim_file", "TKM Animation");

	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
}


// -- Public methods -- //

SideMenu::SideMenu()
{
	m_keybinds = &Keybinds::GetKeybinds();

#ifdef BUILD_TYPE_DEBUG
	m_auto_update_check = false;
	m_auto_addr_update_check = false;
#else
	m_auto_update_check = Settings::Get(SETTING_AUTO_UPDATE_KEY, SETTING_AUTO_UPDATE);
	m_auto_addr_update_check = Settings::Get(SETTING_AUTO_ADDR_UPDATE_KEY, SETTING_AUTO_ADDR_UPDATE);
#endif

	m_vsync_setting = Settings::Get(SETTING_VSYNC_BUFFER_KEY, SETTING_VSYNC_BUFFER);
	m_overwriteSameFilename = Settings::Get(SETTING_EXPORT_OVERWRITE_KEY, SETTING_EXPORT_OVERWRITE);
	m_freeUnusedMoveset = Settings::Get(SETTING_FREE_UNUSED_KEY, SETTING_FREE_UNUSED);
	m_applyInstantly = Settings::Get(SETTING_IMPORT_INSTANT_KEY, SETTING_IMPORT_INSTANT);
	m_languageId = Localization::GetCurrLangId();
	Localization::GetTranslationList(&m_translations, &m_translations_count);
}


SideMenu::~SideMenu()
{
	CleanupThread();
}

void SideMenu::RenderSettingsMenu()
{
	// General settings
	ImGui::SeparatorText(_("settings.general"));
	if (ImGui::BeginTable("GeneralSettingsTable", 2, ImGuiTableFlags_RowBg))
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		// Language list
		ImGui::TextUnformatted(_("settings.language"));
		ImGui::PushItemWidth(200.f);
		if (ImGui::BeginCombo("##", m_translations[m_languageId].displayName))
		{
			for (unsigned int i = 0; i < m_translations_count; ++i)
			{
				ImGui::PushID(&i);
				if (ImGui::Selectable(m_translations[i].displayName, i == m_languageId)) {
					if (Localization::LoadFile(m_translations[i].locale, true)) {
						m_languageId = Localization::GetCurrLangId();
						Settings::Set(SETTING_LANG_KEY, m_languageId);
					}
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();

		// Vsync dropdown
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(_("settings.vsync"));
		char buf[2] = { '0' + m_vsync_setting , 0 };
		ImGui::PushID(&m_vsync_setting);
		ImGui::PushItemWidth(100.f);
		if (ImGui::BeginCombo("##", buf))
		{
			for (unsigned int i = 0; i < 5; ++i)
			{
				ImGui::PushID(i);
				buf[0] = '0' + i;
				if (ImGui::Selectable(buf, i == m_vsync_setting)) {
					m_vsync_setting = i;
					glfwSwapInterval(i);
					Settings::Set(SETTING_VSYNC_BUFFER_KEY, i);
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
		ImGui::PopID();
		ImGui::PopItemWidth();

		ImGui::SameLine();
		ImGuiExtra::HelpMarker(_("settings.vsync_explanation"));

		// Auto-update program & addresses
		{
#ifdef BUILD_TYPE_DEBUG
			// Automatic updates are disabled in debug builds
			ImGuiExtra::DisableBlockIf __(true);
#endif

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (ImGui::Checkbox(_("settings.auto_update_check"), &m_auto_update_check)) {
				Settings::Set(SETTING_AUTO_UPDATE_KEY, m_auto_update_check);
			}
			ImGui::TableNextColumn();
			if (ImGui::Checkbox(_("settings.auto_addr_update_check"), &m_auto_addr_update_check)) {
				Settings::Set(SETTING_AUTO_ADDR_UPDATE_KEY, m_auto_addr_update_check);
			}
		}


		// File association
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (ImGui::Button(_("settings.file_association"))) {
			SetWindowsFileAssociation();
		}
		ImGui::SameLine();
		ImGuiExtra::HelpMarker(_("settings.file_association_explanation"));

		ImGui::EndTable();
	}

	// Importation settings
	ImGui::SeparatorText(_("settings.importation"));
	if (ImGui::BeginTable("ImportSettingsTable", 2, ImGuiTableFlags_RowBg))
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		// Apply movesets instantly
		if (ImGui::Checkbox(_("settings.apply_instantly"), &m_applyInstantly)) {
			Settings::Set(SETTING_IMPORT_INSTANT_KEY, m_applyInstantly);
		}
		ImGui::SameLine();
		ImGuiExtra::HelpMarker(_("settings.apply_instantly_explanation"));

		// Free old unused movesets when importing new ones
		ImGui::TableNextColumn();
		if (ImGui::Checkbox(_("settings.free_unused_movesets"), &m_freeUnusedMoveset)) {
			Settings::Set(SETTING_FREE_UNUSED_KEY, m_freeUnusedMoveset);
		}
		ImGui::SameLine();
		ImGuiExtra::HelpMarker(_("settings.free_unused_movesets_explanation"));

		ImGui::EndTable();
	}

	// Exportation settings
	ImGui::SeparatorText(_("settings.exportation"));
	if (ImGui::BeginTable("ExportSettingsTable", 2, ImGuiTableFlags_RowBg))
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		// Ovewrite movesets with same name
		if (ImGui::Checkbox(_("settings.overwrite_duplicate"), &m_overwriteSameFilename)) {
			Settings::Set(SETTING_EXPORT_OVERWRITE_KEY, m_overwriteSameFilename);
		}
		ImGui::SameLine();
		ImGuiExtra::HelpMarker(_("settings.overwrite_explanation"));

		ImGui::EndTable();
	}


	ImGui::SeparatorText(_("settings.keybinds"));
	if (ImGui::BeginTable("KeybindsSettingsTable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoHostExtendY, ImVec2(0, 300)))
	{

		bool row_end = true;
		auto key_cursor = m_keybinds->begin();
		for (unsigned int i = 0; i < m_keybinds->size(); ++i)
		{
			auto& [identifier, keys] = *key_cursor;

			if (row_end) {
				ImGui::TableNextRow();
			}
			row_end = !row_end;
			ImGui::TableNextColumn();

			ImGui::TextUnformatted(identifier.c_str());
			std::string t;
			for (auto& k : keys) {
				t += ImGui::GetKeyName(k);
			}

			if (m_rebindingIndex == i)
			{
				std::string keys_text;
				for (auto k : m_currentBindingKeys)
				{
					if (keys_text.size() != 0) {
						keys_text += " + ";
					}
					keys_text += ImGui::GetKeyName(k);
				}
				ImGui::TextUnformatted(keys_text.c_str());
			}
			else {
				std::string keys_text;
				for (auto k : keys)
				{
					if (keys_text.size() != 0) {
						keys_text += " + ";
					}
					keys_text += ImGui::GetKeyName(k);
				}
				if (ImGuiExtra::RenderButtonEnabled(keys_text.c_str(), m_rebindingIndex == -1)) {
					m_currentBindingKeys.clear();
					m_rebindingIndex = i;
				}
			}

			std::advance(key_cursor, 1);
		}

		ImGui::EndTable();
	}

	ImGui::Separator();
	if (ImGuiExtra::RenderButtonEnabled(_("settings.reset_keybinds"), m_rebindingIndex == -1)) {
		Keybinds::ApplyDefaultKeybinds();
	}

	if (m_rebindingIndex != -1)
	{
		ImGui::SameLine();
		if (ImGui::Button(_("settings.finish_keybind")))
		{
			auto key_cursor = m_keybinds->begin();
			std::advance(key_cursor, m_rebindingIndex);
			key_cursor->second = m_currentBindingKeys;
			m_rebindingIndex = -1;
		}
		else {
			// Only detect keypresses after allowing detecting keybinds end
			DetectKeypresses(m_currentBindingKeys);
		}
	}

	ImGui::SeparatorText("");
	if (ImGuiExtra::RenderButtonEnabled(_("close"), m_rebindingIndex == -1))
	{
		ImGui::CloseCurrentPopup();
		m_settingsMenuOpen = false;
	}
}

void SideMenu::RenderChangelog()
{
	ImGui::TextUnformatted(_("sidemenu.update_explanation"));

	// Controls
	if (ImGui::Button(_("yes"))) {
		if (DownloadProgramUpdate(&m_updateStatus, m_addresses, false)) {
			*requestedUpdatePtr = true;
		}
		else {
			m_updateFileInvalid = true;
		}

	}
	ImGui::SameLine();
	if (ImGui::Button(_("no")) || ImGui::IsKeyDown(ImGuiKey_Escape)) {
		ImGui::CloseCurrentPopup();
		m_updateStatus.programUpdateAvailable = false;
	}

	if (m_updateFileInvalid) {
		ImGuiExtra_TextboxError(_("sidemenu.update_bad_file"));
	}

	ImGui::SeparatorText(m_updateStatus.tagNameSeparatorText.c_str());

	// Markdown content
	ImGui::SameLine();
	ImGuiExtra::Markdown(m_updateStatus.changelog);

	ImGui::NewLine();
	// Purposeful empty SeparatorText for the thicker line
	ImGui::SeparatorText("");
}

void SideMenu::Render(float width)
{
	if (ImGui::Button(_("sidemenu.settings"), ImVec2(width, 0))) {
		m_settingsMenuOpen = true;
	}

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
			ImGui::TextColored(ImVec4(0, 1.0f, 0, 1), _("sidemenu.up_to_date"));
		}
		else {
			if (m_updateStatus.addrFile) {
				ImGui::TextColored(ImVec4(0, 1.0f, 0, 1), _("sidemenu.updated_addr"));
			}
		}

	}

	// Update popup
	if (m_updateStatus.programUpdateAvailable) {
		ImGui::OpenPopup("ProgramUpdatePopup");
		ImGui::SetNextWindowSizeConstraints(ImVec2(500, 200), ImVec2(720, 720));
	}
	if (ImGui::BeginPopupModal("ProgramUpdatePopup", &m_updateStatus.programUpdateAvailable))
	{
		RenderChangelog();
		ImGui::EndPopup();
	}

	// Update popup
	if (m_settingsMenuOpen) {
		ImGui::OpenPopup("SettingsMenu");
		ImGui::SetNextWindowSizeConstraints(ImVec2(720, 640), ImVec2(-1, -1));
	}
	if (ImGui::BeginPopupModal("SettingsMenu", &m_settingsMenuOpen))
	{
		RenderSettingsMenu();
		ImGui::EndPopup();
	}
}