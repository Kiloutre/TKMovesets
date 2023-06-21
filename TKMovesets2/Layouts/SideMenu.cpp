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
		if (dwType != REG_SZ)
			DEBUG_ERR("RegGetValue( %S ) found type unhandled %d", pszVar, dwType);

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
#ifdef BUILD_TYPE_DEBUG
	m_auto_update_check = false;
	m_auto_addr_update_check = false;
#else
	m_auto_update_check = Settings::Get(SETTING_AUTO_UPDATE_KEY, SETTING_AUTO_UPDATE);
	m_auto_addr_update_check = Settings::Get(SETTING_AUTO_ADDR_UPDATE_KEY, SETTING_AUTO_ADDR_UPDATE);
#endif

	m_vsync_setting = Settings::Get(SETTING_VSYNC_BUFFER_KEY, SETTING_VSYNC_BUFFER);
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
					Settings::Set(SETTING_LANG_KEY, m_languageId);
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
			ImGui::TextColored(ImVec4(0, 1.0f, 0, 1), _("sidemenu.up_to_date"));
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
				Settings::Set(SETTING_VSYNC_BUFFER_KEY, i);
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
		if (ImGui::Button(_("no")) || ImGui::IsKeyDown(ImGuiKey_Escape)) {
			ImGui::CloseCurrentPopup();
			m_updateStatus.programUpdateAvailable = false;
		}

		if (m_updateFileInvalid) {
			ImGuiExtra_TextboxError(_("sidemenu.update_bad_file"));
		}

		ImGui::EndPopup();
	}

	
	ImGui::Separator();
	if (ImGui::Button(_("sidemenu.file_association"))) {
		SetWindowsFileAssociation();
	}

	{
#ifdef BUILD_TYPE_DEBUG
		// Automatic updates are disabled in debug builds
		ImGuiExtra::DisableBlockIf __(true);
#endif

		if (ImGui::Checkbox(_("sidemenu.auto_update_check"), &m_auto_update_check)) {
			Settings::Set(SETTING_AUTO_UPDATE_KEY, m_auto_update_check);
		}
		if (ImGui::Checkbox(_("sidemenu.auto_addr_update_check"), &m_auto_addr_update_check)) {
			Settings::Set(SETTING_AUTO_ADDR_UPDATE_KEY, m_auto_addr_update_check);
		}
	}
}