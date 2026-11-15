#include <ImGui.h>
#include "GLFW/glfw3.h"
#include "shlobj_core.h"

#include "Settings.hpp"
#include "SideMenu.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "helpers.hpp"
#include "MovesetFolders.hpp"
#include "LocalStorage.hpp"
#include <shobjidl.h>

namespace {
void BrowseFolder(char* buffer,size_t capacity) {
    const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(initialized) && initialized!=RPC_E_CHANGED_MODE)return;
    IFileOpenDialog* dialog=nullptr;
    if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) {
        dialog->SetOptions(FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST|FOS_NOCHANGEDIR);
        if(SUCCEEDED(dialog->Show(GetActiveWindow()))) {
            IShellItem* item=nullptr;
            if(SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR path=nullptr;
                if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))) {
                    auto text=Helpers::to_utf8(path);snprintf(buffer,capacity,"%s",text.c_str());CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dialog->Release();
    }
    if(SUCCEEDED(initialized))CoUninitialize();
}
}

void SideMenu::RenderFolderSettings() {
    ImGui::SeparatorText(_("settings.folders.title"));
    ImGui::TextUnformatted(_("settings.folders.extraction"));
    ImGui::SetNextItemWidth(-200);
    ImGui::InputTextWithHint("##extractionFolder",_("settings.folders.extraction_hint"),m_extractionFolder,sizeof(m_extractionFolder));
    ImGui::SameLine();if(ImGui::Button((std::string(_("settings.folders.browse"))+"##output").c_str()))BrowseFolder(m_extractionFolder,sizeof(m_extractionFolder));
    ImGui::SameLine();if(ImGui::Button((std::string(_("settings.folders.save"))+"##output").c_str())) {
        try {auto c=MovesetFolders::Get();c.extraction=m_extractionFolder;MovesetFolders::Save(std::move(c));m_folderError.clear();}
        catch(const std::exception& e){m_folderError=e.what();}
    }
    ImGui::SameLine();if(ImGui::Button((std::string(_("settings.folders.clear"))+"##output").c_str())) {
        auto c=MovesetFolders::Get();c.extraction.clear();MovesetFolders::Save(std::move(c));m_extractionFolder[0]=0;m_folderError.clear();
    }
    ImGui::TextWrapped("%s",_("settings.folders.extraction_help"));
    ImGui::Spacing();ImGui::TextUnformatted(_("settings.folders.additional"));
    ImGui::TextWrapped("%s",_("settings.folders.additional_help"));
    auto c=MovesetFolders::Get();
    // Any change rebuilds the character list right away (a removed folder's files must leave the list too).
    auto refresh=[&]{if(storage)storage->RefreshMovesets();};
    for(size_t i=0;i<c.additional.size();++i) {
        ImGui::PushID(static_cast<int>(i));
        if(ImGui::Button(_("settings.folders.remove"))) {c.additional.erase(c.additional.begin()+i);MovesetFolders::Save(c);refresh();ImGui::PopID();break;}
        ImGui::SameLine();ImGui::TextWrapped("%s",c.additional[i].c_str());ImGui::PopID();
    }
    if(ImGui::Button((std::string(_("settings.folders.add"))+"##search").c_str())) {
        // Pick a folder in the explorer: it is registered immediately.
        m_additionalFolder[0]=0;
        BrowseFolder(m_additionalFolder,sizeof(m_additionalFolder));
        if(m_additionalFolder[0]) try {
            auto path=std::filesystem::u8path(m_additionalFolder);
            if(!path.is_absolute() || !std::filesystem::is_directory(path))throw std::invalid_argument(_("settings.folders.missing"));
            bool duplicate=false;
            for(const auto& existing:c.additional) {std::error_code ec;if(std::filesystem::equivalent(path,std::filesystem::u8path(existing),ec))duplicate=true;}
            if(!duplicate){c.additional.push_back(m_additionalFolder);MovesetFolders::Save(c);refresh();}
            m_folderError=duplicate?_("settings.folders.duplicate"):"";
        }catch(const std::exception& e){m_folderError=e.what();}
        m_additionalFolder[0]=0;
    }
    ImGui::SameLine();ImGui::TextDisabled("%s",_("settings.folders.add_help"));
    if(!m_folderError.empty())ImGui::TextWrapped("%s",m_folderError.c_str());
}

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

	// Older versions also associated the temporary file an interrupted extraction leaves behind.
	// That file is not a valid moveset, so remove the stale association if it is still ours.
	{
		const char* tmpKey = "Software\\Classes\\" MOVESET_TMPFILENAME_EXTENSION;
		char current[64] = { 0 };
		DWORD size = sizeof(current);
		if (RegGetValueA(HKEY_CURRENT_USER, tmpKey, NULL, RRF_RT_REG_SZ, NULL, current, &size) == ERROR_SUCCESS
			&& strcmp(current, "tkmvst_file") == 0) {
			RegDeleteTreeA(HKEY_CURRENT_USER, tmpKey);
		}
	}

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
    MovesetFolders::Load();
    snprintf(m_extractionFolder,sizeof(m_extractionFolder),"%s",MovesetFolders::Get().extraction.c_str());
	m_keybinds = &Keybinds::GetKeybinds();

#ifdef BUILD_TYPE_DEBUG
	m_auto_update_check = false;
	m_auto_addr_update_check = false;
#else
	m_auto_update_check = Settings::Get(SETTING_AUTO_UPDATE_KEY, SETTING_AUTO_UPDATE);
	m_auto_addr_update_check = Settings::Get(SETTING_AUTO_ADDR_UPDATE_KEY, SETTING_AUTO_ADDR_UPDATE);
#endif

	m_vsync_setting = Settings::Get(SETTING_VSYNC_BUFFER_KEY, SETTING_VSYNC_BUFFER);
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

    RenderFolderSettings();
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


	ImGui::SeparatorText(_("settings.keybinds"));
	// Fit the table to its rows; only scroll once there are more than a handful of keybinds.
	const size_t keybindRows = (m_keybinds->size() + 1) / 2;
	const size_t visibleKeybindRows = 6;
	const float keybindRowHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetFrameHeightWithSpacing();
	const bool scrollKeybinds = keybindRows > visibleKeybindRows;
	ImGuiTableFlags keybindTableFlags = ImGuiTableFlags_RowBg;
	if (scrollKeybinds) keybindTableFlags |= ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoHostExtendY;
	if (ImGui::BeginTable("KeybindsSettingsTable", 2, keybindTableFlags,
		ImVec2(0, scrollKeybinds ? keybindRowHeight * visibleKeybindRows : 0)))
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

			ImGui::TextUnformatted(_(identifier.c_str()));
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
		Settings::SaveSettingsFile();
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
			Settings::SaveSettingsFile();
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

	if (!m_updateStatus.verifying && m_updateStatus.error) {
		ImGui::TextColored(ImVec4(1.0f, 0, 0, 1), _("sidemenu.update_error"));
	}
	else {
		if (!m_updateStatus.verifying && m_updateStatus.up_to_date) {
			ImGui::TextColored(ImVec4(0, 1.0f, 0, 1), _("sidemenu.up_to_date"));
		}
		else {
			if (!m_updateStatus.verifying && m_updateStatus.addrFile) {
				ImGui::TextColored(ImVec4(0, 1.0f, 0, 1), _("sidemenu.updated_addr"));
			}
		}

	}

	// Update popup
	if (!m_updateStatus.verifying && m_updateStatus.programUpdateAvailable) {
		ImGui::OpenPopup("###ProgramUpdatePopup");
	}
	if (!m_updateStatus.verifying && ImGuiExtra::BeginFittedModal("ProgramUpdatePopup", _("sidemenu.update"), &m_updateStatus.programUpdateAvailable, 40.0f))
	{
		RenderChangelog();
		ImGui::EndPopup();
	}

	// Update popup
	if (m_settingsMenuOpen) {
		ImGui::OpenPopup("###SettingsMenu");
	}
	if (ImGuiExtra::BeginFittedModal("SettingsMenu", _("sidemenu.settings"), &m_settingsMenuOpen, 46.0f))
	{
		RenderSettingsMenu();
		ImGui::EndPopup();
	}
}
