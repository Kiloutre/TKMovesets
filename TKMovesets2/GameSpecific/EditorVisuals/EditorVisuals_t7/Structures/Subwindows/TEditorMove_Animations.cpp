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
	DEBUG_LOG("ApplySearchFilter\n");
	std::vector<AnimationLibChar*> characters = m_characters;
	for (auto& character : characters)
	{
		if (strlen(m_searchBuffer) == 0) {
			character->filteredFiles = character->files;
		}
		else {
			auto& filteredFiles = character->filteredFiles;

			filteredFiles.clear();

			std::vector<AnimationLibFile*> files = character->files;
			for (auto& file : files)
			{
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

static size_t GetFileSize(std::ifstream& file)
{
	file.seekg(0, std::ios::end);
	size_t retVal = file.tellg();
	file.seekg(0, std::ios::beg);
	return retVal;
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

		std::ifstream animListFile(characterFolder + L"/anims.txt");

		AnimationLibChar* charAnims = new AnimationLibChar;
		bool addedFile = false;
		charAnims->name = characterName;

		if (!animListFile.fail() && GetFileSize(animListFile) != 0)
		{
			// Use pre-computed anims.txt if it exists
			std::string line;
			while (std::getline(animListFile, line))
			{
				auto i = line.find(",");
				auto i2 = line.find(",", i + 1);
				auto i3 = line.find(",", i2 + 1);
				
				if (i == std::string::npos || i2 == std::string::npos || i3 == std::string::npos) {
					DEBUG_ERR("%s anims.txt : Bad line formatting: '%s'", characterName.c_str(), line.c_str());
					continue;
				}

				std::string name = line.substr(0, i);
				std::string lowercaseName = name;
				std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(), tolower);
				unsigned int file_size = std::atoi(line.c_str() + i2 + 1);

				auto anim = new AnimationLibFile{
					.name = name,
					.lowercaseName = lowercaseName,
					.filepath = characterFolder + L"/" + Helpers::to_unicode(line.substr(0, i)),
					.duration = line.substr(i + 1, i2 - i - 1),
					.size_kilobytes = std::format("{:.2f}", ((float)file_size) / 1000.0f),
					.size = file_size,
					.hash = line.substr(i3 + 1)
				};

				charAnims->files.push_back(anim);
				m_animCount++;

				if (!addedFile) {
					DEBUG_LOG("Adding pre-computed animation list from character %s\n", charAnims->name.c_str());
					m_characters.push_back(charAnims);
					addedFile = true;
				}
			}
		}
		else
		{
			// Fallback to file listing and animation analyzing

			s_animInfo animInfo;
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
				std::string lowercaseName = name;
				std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(), tolower);

				if (!TAnimUtils::FromFile::GetAnimationInfos(filename.c_str(), animInfo)) {
					// Probably an invalid file. todo : show, but prevent import?
					continue;
				}

				auto anim = new AnimationLibFile{
					.name = name,
					.lowercaseName = lowercaseName,
					.filepath = filename,
					.duration = std::to_string(animInfo.duration),
					.size_kilobytes = std::format("{:.2f}", ((float)file.file_size()) / 1000.0f),
					.size = (unsigned int)file.file_size(),
					.hash = std::format("{:08X}", animInfo.hash)
				};
				charAnims->files.push_back(anim);
				m_animCount++;

				if (!addedFile) {
					DEBUG_LOG("Adding computed animation list from character %s\n", charAnims->name.c_str());
					m_characters.push_back(charAnims);
					addedFile = true;
				}

			}

			if (!m_destructionRequested)
			{
				// Write new anims.txt with newly computed informations
				std::ofstream newAnimListFile(characterFolder + L"/anims.txt");
				if (newAnimListFile.fail()) {
					DEBUG_ERR("Failed to create '%S'", (characterFolder + L"/anims.txt").c_str());
				}
				else {
					for (auto anim : charAnims->files) {
						newAnimListFile << anim->name << "," << anim->duration << "," << anim->size << "," << anim->hash << std::endl;
					}
					DEBUG_LOGN("Successfully wrote %llu animations to '%S'", charAnims->files.size(), (characterFolder + L"/anims.txt").c_str());
				}
			}
		}

		if (charAnims->files.size() == 0) {
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
	m_justFinishedLoading = true;
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

		if (m_justFinishedLoading || (m_animCount != m_prevRenderAnimCount)) {
			if (m_justFinishedLoading) {
				m_justFinishedLoading = false;
			}
			m_prevRenderAnimCount = m_animCount;
			ApplySearchFilter();
			// Animation was added, re-compute animation list
		}

		ImGui::SameLine();
		if (ImGui::Button(_("edition.animation_list.clear_filter"))) {
			m_searchBuffer[0] = '\0';
			m_lowercaseBuffer = std::string();
			ApplySearchFilter();
		}

		ImGui::SameLine();
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

		if (ImGui::InputTextWithHint("##", _("edition.animation_list.search_animation"), m_searchBuffer, sizeof(m_searchBuffer))) {
			m_lowercaseBuffer = std::string(m_searchBuffer);
			std::transform(m_lowercaseBuffer.begin(), m_lowercaseBuffer.end(), m_lowercaseBuffer.begin(), tolower);
			ApplySearchFilter();
		}

		ImGui::PopItemWidth();

		if (!loadedList) {
			ImGui::TextUnformatted(_("edition.animation_list.loading"));
		} else if (m_characters.size() == 0) {
			ImGuiExtra::RenderTextbox(_("edition.animation_list.no_anim"));
		}

		// Cache these because animation listing might get executed dozens of times (once per each moveset with extracted anims)
		const char* animation_list__anim_name = _("edition.animation_list.anim_name");
		const char* animation_list__duration = _("edition.animation_list.duration");
		const char* animation_list__size = _("edition.animation_list.size");
		const char* animation_list__hash = _("hash");
		const char* moveset__size_kb = _("moveset.size_kb");
		const char* animation_list__import = _("edition.animation_list.import");

		for (int i = 0; i < m_characters.size(); ++i)
		{
			const auto& character = m_characters[i];
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
				if (ImGui::BeginTable("##", 5, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable, ImVec2(ImGui::GetContentRegionAvail().x, 0)))
				{
					ImGui::TableSetupColumn(animation_list__anim_name);
					ImGui::TableSetupColumn(animation_list__duration);
					ImGui::TableSetupColumn(animation_list__size);
					ImGui::TableSetupColumn(animation_list__hash);
					ImGui::TableSetupColumn("##");
					ImGui::TableHeadersRow();

					for (const auto& file : character->filteredFiles)
					{
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(file->name.c_str());

						ImGui::TableNextColumn();
						ImGui::TextUnformatted(file->duration.c_str());

						ImGui::TableNextColumn();
						ImGui::TextUnformatted(file->size_kilobytes.c_str());
						ImGui::SameLine();
						ImGui::TextUnformatted(moveset__size_kb);

						ImGui::TableNextColumn();
						ImGui::TextUnformatted(file->hash.c_str());

						ImGui::TableNextColumn();
						ImGui::PushID(file->filepath.c_str());
						if (ImGui::Button(animation_list__import)) {
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