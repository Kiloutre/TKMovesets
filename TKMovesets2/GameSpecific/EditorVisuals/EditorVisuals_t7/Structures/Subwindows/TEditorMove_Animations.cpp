#include <ImGui.h>
#include <filesystem>
#include <fstream>

#include "tEditorMove_Animations.hpp"
#include "Localization.hpp"
#include "Helpers.hpp"
#include "imgui_extras.hpp"
#include "Animations.hpp"

#include "constants.h"

// -- Static helpers -- //

void TEditorMove_Animations::ApplySearchFilter()
{
	for (unsigned int j = 0; j < m_characters.size(); ++j)
	{
		auto& character = m_characters[j];
		if (strlen(m_searchBuffer) == 0) {
			character->filteredFiles = character->files;
		}
		else {
			auto& filteredFiles = character->filteredFiles;
			filteredFiles.clear();

			for (unsigned int i = 0; i < character->files.size(); ++i)
			{
				auto& file = character->files[i];
				if (strstr(file->lowercaseName.c_str(), m_lowercaseBuffer.c_str())) {
					filteredFiles.push_back(file);
					character->forceOpen = true;
				}
			}

			if (!character->forceOpen) {
				character->forceClose = true;
			}
		}
	}

}

TEditorMove_Animations::TEditorMove_Animations()
{
	m_loadingThread = std::thread(&TEditorMove_Animations::LoadAnimationList, this);
}

TEditorMove_Animations::~TEditorMove_Animations()
{
	if (!loadedList) {
		m_destructionRequested = true;
	}
	m_loadingThread.join();
	for (auto& charAnims : m_characters) {
		for (auto& file : charAnims->files) {
			delete file;
		}
		delete charAnims;
	}
}

void TEditorMove_Animations::LoadAnimationList()
{
	CreateDirectory(EDITOR_LIB_DIRECTORY, nullptr);

	for (const auto& directory : std::filesystem::directory_iterator(EDITOR_LIB_DIRECTORY))
	{
		if (!directory.is_directory()) {
			continue;
		}

		std::wstring characterFolder = directory.path().wstring();
		std::string characterName = Helpers::wstring_to_string(characterFolder.substr(characterFolder.find_last_of(L"/\\")));

		AnimationLibChar* charAnims = new AnimationLibChar;
		charAnims->name = characterName;
		int fileIdx = 0;

		for (const auto& file : std::filesystem::directory_iterator(characterFolder))
		{
			if (m_destructionRequested) {
				break;
			}

			if (!file.is_regular_file()) {
				continue;
			}

			std::wstring filename = file.path().wstring();

			if (!Helpers::endsWith<std::wstring>(filename, L".bin") &&
				!Helpers::endsWith<std::wstring>(filename, L"" ANIMATION_EXTENSION L"C8") &&
				!Helpers::endsWith<std::wstring>(filename, L"" ANIMATION_EXTENSION L"64")
				) {
				continue;
			}

			std::string name = Helpers::to_utf8(filename.substr(filename.find_last_of(L"/\\") + 1));
			name = name.substr(0, name.find_last_of('.'));
			std::string lowercaseName = name;
			std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(), tolower);

			int duration = TAnimUtils::FromFile::GetAnimationDuration(filename.c_str());

			if (duration == -1) {
				// Probably an invalid file. todo : show, but prevent import?
				continue;
			}

			charAnims->files.push_back(new AnimationLibFile{
				.name = name,
				.lowercaseName = lowercaseName,
				.filepath = filename,
				.duration = std::to_string(duration),
				.size_megabytes = std::format("{:.2f}", ((float)file.file_size()) / 1000.0f)
			});

			charAnims->filteredFiles.push_back(charAnims->files[fileIdx]);
			if (fileIdx == 0) {
				m_characters.push_back(charAnims);
			}
			++fileIdx;
		}

		if (fileIdx == 0) {
			delete charAnims;
		}
		else {
			charAnims->name = std::format("{} - {}", charAnims->name, charAnims->files.size());
		}

		if (m_destructionRequested) {
			break;
		}
	}

	loadedList = true;
}

bool TEditorMove_Animations::Render()
{
	ImGui::SetNextWindowSizeConstraints(ImVec2(720, 640), ImVec2(9999, 9999));
	if (ImGui::BeginPopupModal("AnimListPopup", &popen))
	{
		if (ImGui::Button(_("close_window")) || ImGui::IsKeyDown(ImGuiKey_Escape)) {
			ImGui::CloseCurrentPopup();
			popen = false;
		}

		ImGui::SameLine();
		bool disabled = !loadedList;
		if (ImGui::Button(_("edition.animation_list.clear_filter"))) {
			m_searchBuffer[0] = '\0';
			m_lowercaseBuffer = std::string();
			ApplySearchFilter();
		}

		ImGui::SameLine();
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

		if (disabled) {
			ImGui::BeginDisabled();
		}
		if (ImGui::InputTextWithHint("##", _("edition.animation_list.search_animation"), m_searchBuffer, sizeof(m_searchBuffer))) {
			m_lowercaseBuffer = std::string(m_searchBuffer);
			std::transform(m_lowercaseBuffer.begin(), m_lowercaseBuffer.end(), m_lowercaseBuffer.begin(), tolower);
			ApplySearchFilter();
		}

		ImGui::PopItemWidth();

		if (disabled) {
			ImGui::EndDisabled();
			ImGui::TextUnformatted(_("edition.animation_list.loading"));
		} else if (m_characters.size() == 0) {
			ImGuiExtra::RenderTextbox(_("edition.animation_list.no_anim"));
		}

		for (int i = 0; i < m_characters.size(); ++i)
		{
			auto& character = m_characters[i];
			if (character->filteredFiles.size() == 0) {
				continue;
			}

			if (character->forceOpen || character->forceClose) {
				ImGui::SetNextItemOpen(character->forceOpen ? true : false);
				character->forceOpen = false;
				character->forceClose = false;
			}

			if (ImGui::TreeNode(character->name.c_str()))
			{
				if (ImGui::BeginTable("##", 4, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable, ImVec2(ImGui::GetContentRegionAvail().x, 0)))
				{
					ImGui::TableSetupColumn(_("edition.animation_list.anim_name"));
					ImGui::TableSetupColumn(_("edition.animation_list.duration"));
					ImGui::TableSetupColumn(_("edition.animation_list.size"));
					ImGui::TableSetupColumn("##");
					ImGui::TableHeadersRow();

					for (int j = 0; j < character->filteredFiles.size(); ++j)
					{
						auto& file = character->filteredFiles[j];
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(file->name.c_str());

						ImGui::TableNextColumn();
						ImGui::TextUnformatted(file->duration.c_str());

						ImGui::TableNextColumn();
						ImGui::TextUnformatted(file->size_megabytes.c_str());
						ImGui::SameLine();
						ImGui::TextUnformatted(_("moveset.size_kb"));

						ImGui::TableNextColumn();
						ImGui::PushID(file->filepath.c_str());
						if (ImGui::Button(_("edition.animation_list.import"))) {
							animationToImport = file->filepath.c_str();
							// Value will be caught by parent window
						}
						ImGui::PopID();

					}
					ImGui::EndTable();
				}
				ImGui::TreePop();
			}
		}

		ImGui::EndPopup();
	}

	return popen;
}