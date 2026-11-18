#include "FileIO.hpp"
#include <fstream>
#include <ImGui.h>
#include <filesystem>
#include <system_error>
#include <cstring>

#include "Submenu_Edition.hpp"
#include "Localization.hpp"
#include "MovesetFolders.hpp"
#include "MovesetListSort.hpp"
#include "imgui_extras.hpp"
#include "helpers.hpp"
#include "MovesetConverters/MovesetFileConverter.hpp"
#include "AnimExtractors/AnimExtractors.hpp"
#include "AnimExtractors/AnimExtractor_t8.hpp"
#include "Extractor_t8/RetailDiskMoveset.hpp"
#include "Extractor_t8/RetailGameData.hpp"
#include "GameStaticData.hpp"
#include <algorithm>
#include <cwctype>

// -- Static helpers -- //

// Lower-cased wide copy of a UTF-8 string, for case-insensitive search (works for non-Latin names too)
static std::wstring LowerForSearch(const std::string& utf8)
{
	std::wstring text = Helpers::to_unicode(utf8.c_str());
	std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); });
	return text;
}

// True if the moveset's name, game or target character contains the (lower-cased) search text
static bool MatchesSearch(const movesetInfo& moveset, const std::wstring& needle)
{
	for (const char* field : { moveset.name.c_str(), moveset.origin.c_str(), moveset.target_character.c_str() }) {
		if (LowerForSearch(field).find(needle) != std::wstring::npos) return true;
	}
	return false;
}

static bool IsConversionTarget(const GameInfo& game, uint16_t sourceGameId)
{
	return CanConvertMoveset(sourceGameId, game.gameId, game.minorVersion);
}

// Copies a UTF-8 string into a fixed buffer, cutting it at a code point boundary when too long (strcpy_s would abort the process)
static void CopyUtf8Truncated(char* destination, size_t destinationSize, const std::string& source)
{
	if (destinationSize == 0) return;
	size_t length = (std::min)(source.size(), destinationSize - 1);
	if (length < source.size()) {
		// Don't keep the lead bytes of a character whose continuation bytes were cut off
		while (length > 0 && (static_cast<unsigned char>(source[length]) & 0xC0) == 0x80) --length;
	}
	memcpy(destination, source.data(), length);
	destination[length] = '\0';
}

static bool HasConversionTarget(uint16_t sourceGameId)
{
	const auto gameCount = Games::GetGamesCount();
	for (unsigned int i = 0; i < gameCount; ++i) {
		if (IsConversionTarget(*Games::GetGameInfoFromIndex(i), sourceGameId)) {
			return true;
		}
	}
	return false;
}

void DuplicateMoveset(std::wstring filename)
{
	size_t extPos = filename.find_last_of(L".");
	std::wstring name = filename.substr(0, extPos);
	std::wstring extension = filename.substr(extPos);

	unsigned int number = 2;
	std::wstring new_name = name + L" (" + std::to_wstring(number) + L")" + extension;
	while (Helpers::fileExists(new_name.c_str()))
	{
		++number;
		new_name = name + L" (" + std::to_wstring(number) + L")" + extension;
	}

	try {
		std::filesystem::copy_file(filename, new_name);
	} catch (std::exception& _) {
        DEBUG_ERR("Failed to duplicate moveset");
		(void)_;
		// Todo: maybe an error popup of some kind
	}
}

// -- Public methods -- //

static RenameErrcode_ RenameMoveset(std::wstring full_filename, const char* newName)
{
	if (strlen(newName) == 0) {
		return RenameErrcode_EmptyName;
	}

	std::wstring w_newName;
	try {
		w_newName = Helpers::to_unicode(newName);
	} catch (const std::exception&) {
		return RenameErrcode_InvalidName; // Not valid UTF-8
	}
	std::wstring new_full_filename = full_filename.substr(0, full_filename.find_last_of(L"/\\") + 1)
									+ w_newName + (L"" MOVESET_FILENAME_EXTENSION);

	if (std::string(newName).find_first_of("/\\<>:\"|?*") != -1) {
		return RenameErrcode_InvalidName;
	}

	if (Helpers::fileExists(new_full_filename.c_str()) && full_filename != new_full_filename) {
		return RenameErrcode_AlreadyExists;
	}

	std::error_code error;
	std::filesystem::rename(full_filename, new_full_filename, error);
	if (error) {
		DEBUG_ERR("Failed to rename moveset");
		return RenameErrcode_RenameErr; // permissions, a locked file, ...: keep the dialog open with the error
	}


	return RenameErrcode_NoErr;
}

void Submenu_Edition::RenderRenamePopup()
{

	ImGui::InputText(_("edition.rename.new_moveset_name"), m_newName, sizeof(m_newName));

	if (ImGui::Button(_("close")) || ImGui::IsKeyDown(ImGuiKey_Escape)) {
		ImGui::CloseCurrentPopup();
		m_renamePopup = false;
		return;
	}

	ImGui::SameLine();
	if (ImGui::Button(_("edition.save")))
	{
		// An open editor keeps the old path and would recreate the old file on its next save
		currentErr = IsOpenInEditor(m_actionTarget.filename) ? RenameErrcode_OpenInEditor : RenameMoveset(m_actionTarget.filename, m_newName);
		if (currentErr == RenameErrcode_NoErr) {
			ImGui::CloseCurrentPopup();
			m_renamePopup = false;
			return;
		}
	}

	switch (currentErr)
	{
	case RenameErrcode_EmptyName:
		ImGui::TextColored(ImVec4(1, 0, 0.2f, 1), "%s", _("edition.rename.empty_name"));
		break;
	case RenameErrcode_AlreadyExists:
		ImGui::TextColored(ImVec4(1, 0, 0.2f, 1), "%s", _("edition.rename.already_exists"));
		break;
	case RenameErrcode_RenameErr:
		ImGui::TextColored(ImVec4(1, 0, 0.2f, 1), "%s", _("edition.rename.rename_err"));
		break;
	case RenameErrcode_InvalidName:
		ImGui::TextColored(ImVec4(1, 0, 0.2f, 1), "%s", _("edition.rename.invalid_name"));
		break;
	case RenameErrcode_OpenInEditor:
		ImGui::TextColored(ImVec4(1, 0, 0.2f, 1), "%s", _("edition.moveset_open_in_editor"));
		break;
	default:
		break;
	}
}

void Submenu_Edition::RenderConversionPopup()
{
	if (ImGui::Button(_("close")) || ImGui::IsKeyDown(ImGuiKey_Escape)) {
		ImGui::CloseCurrentPopup();
		m_conversionPopup = false;
		return;
	}

    if (!m_conversionMessage.empty()) ImGui::TextUnformatted(m_conversionMessage.c_str());
    if (m_conversionJob.Busy()) {
        ImGui::TextUnformatted(FileJobStageText(m_conversionJob.Stage()));
        if (ImGui::Button(_("edition.convert.cancel"))) m_conversionJob.Cancel();
        return;
    }
	ImGui::SeparatorText(_("edition.convert.format_target"));
    if (m_actionTarget.gameId == GameId_T7)
        ImGui::TextWrapped("%s",_("edition.convert.t7_to_t8_explanation"));
    else if (m_actionTarget.gameId == GameId_T8)
        ImGui::TextWrapped("%s",_("edition.convert.t8_to_t7_explanation"));

	auto gameListCount = Games::GetGamesCount();
	const GameInfo* games = Games::GetGameInfoFromIndex(0);

	ImGui::PushID(games);
	for (unsigned int i = 0; i < gameListCount; ++i)
	{
		if (IsConversionTarget(*games, m_actionTarget.gameId)){
			if (ImGui::Button(games->name)) {
                try {
                    FileJobRequest request;
                    request.kind = games->gameId == GameId_T8 ? FileJobKind::ConvertToT8 : FileJobKind::ConvertToT7;
                    request.source = m_actionTarget.filename;
                    request.output = ConvertedMovesetPath(m_actionTarget, games->gameId);
                    m_conversionJob.Start(std::move(request));
                    m_conversionMessage.clear();
                } catch (const std::exception& error) { m_conversionMessage = error.what(); }
				ImGui::PopID();
				return;
			}
		}
		++games;
	}
	ImGui::PopID();

}

void Submenu_Edition::RenderImportProfilePopup()
{
	using P = ImportProfile::Profile;
	ImGui::TextUnformatted(m_actionTarget.name.c_str());
	ImGui::TextWrapped("%s", _("edition.import_settings.explanation"));
	ImGui::Separator();

	int identity = static_cast<int>(m_profile.identity);
	ImGui::TextUnformatted(_("edition.import_settings.identity"));
	ImGui::RadioButton(_("edition.import_settings.identity_target"), &identity, 0); ImGui::SameLine();
	ImGui::RadioButton(_("edition.import_settings.identity_fixed"), &identity, 1);
	m_profile.identity = static_cast<P::Identity>(identity);
	if (m_profile.identity == P::Identity::Fixed) {
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::InputInt(_("edition.import_settings.character"), &m_profile.character)) m_profile.character = std::clamp(m_profile.character, 0, 255);
	}

	int gender = static_cast<int>(m_profile.gender);
	ImGui::TextUnformatted(_("edition.import_settings.gender"));
	ImGui::RadioButton(_("edition.import_settings.gender_target"), &gender, 0); ImGui::SameLine();
	ImGui::RadioButton(_("edition.import_settings.male"), &gender, 1); ImGui::SameLine();
	ImGui::RadioButton(_("edition.import_settings.female"), &gender, 2);
	m_profile.gender = static_cast<P::Gender>(gender);

	bool storyGame = m_profile.storyFlag == ImportProfile::StoryFlagGameDecides;
	ImGui::TextUnformatted(_("edition.import_settings.story_flag"));
	if (ImGui::Checkbox(_("edition.import_settings.story_game"), &storyGame))
		m_profile.storyFlag = storyGame ? ImportProfile::StoryFlagGameDecides : m_profileStoryFlag;
	if (!storyGame) {
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::InputInt(_("edition.import_settings.story_value"), &m_profileStoryFlag)) m_profileStoryFlag = std::clamp(m_profileStoryFlag, 0, 0xffff);
		m_profile.storyFlag = m_profileStoryFlag;
	}

	ImGui::Checkbox(_("edition.import_settings.in_heat"), &m_profile.inHeat);
	ImGui::Checkbox(_("edition.import_settings.flags_clear"), &m_profile.flagsClear);

	// Context options: every default leaves the checks to the game (or the conversion rules).
	ImGui::SeparatorText(_("edition.import_settings.context"));
	auto toggle = [](const char* label, P::Toggle& value) {
		bool on = value == P::Toggle::Versus;
		if (ImGui::Checkbox(label, &on)) value = on ? P::Toggle::Versus : P::Toggle::Default;
	};
	toggle(_("edition.import_settings.story_mode"), m_profile.storyMode);
	toggle(_("edition.import_settings.dlc_story"), m_profile.dlcStory);
	toggle(_("edition.import_settings.special_modes"), m_profile.specialModes);
	toggle(_("edition.import_settings.stage"), m_profile.stage);
	auto value = [](const char* label, const char* fixedLabel, int& v) {
		bool fixed = v != ImportProfile::ValueGameDecides;
		if (ImGui::Checkbox(label, &fixed)) v = fixed ? 0 : ImportProfile::ValueGameDecides;
		if (fixed) {
			ImGui::SameLine(); ImGui::SetNextItemWidth(100.0f);
			ImGui::PushID(label);
			if (ImGui::InputInt(fixedLabel, &v)) v = (std::max)(v, 0);
			ImGui::PopID();
		}
	};
	value(_("edition.import_settings.wave_index"), _("edition.import_settings.value"), m_profile.waveIndex);
	value(_("edition.import_settings.battle_script"), _("edition.import_settings.value"), m_profile.battleScript);
	int item = static_cast<int>(m_profile.item);
	ImGui::TextUnformatted(_("edition.import_settings.item"));
	ImGui::RadioButton(_("edition.import_settings.game_decides"), &item, 0); ImGui::SameLine();
	ImGui::PushID("item");
	ImGui::RadioButton(_("edition.import_settings.item_none"), &item, 1); ImGui::SameLine();
	ImGui::RadioButton(_("edition.import_settings.item_all"), &item, 2);
	ImGui::PopID();
	m_profile.item = static_cast<P::Item>(item);
	int body = static_cast<int>(m_profile.bodyType);
	ImGui::TextUnformatted(_("edition.import_settings.body"));
	ImGui::PushID("body");
	ImGui::RadioButton(_("edition.import_settings.game_decides"), &body, 0); ImGui::SameLine();
	ImGui::RadioButton(_("edition.import_settings.body_normal"), &body, 1); ImGui::SameLine();
	ImGui::RadioButton(_("edition.import_settings.body_big"), &body, 2); ImGui::SameLine();
	ImGui::RadioButton(_("edition.import_settings.body_bear"), &body, 3);
	ImGui::PopID();
	m_profile.bodyType = static_cast<P::Body>(body);
	auto strip = [](const char* label, P::Strip& value) {
		bool on = value == P::Strip::Strip;
		if (ImGui::Checkbox(label, &on)) value = on ? P::Strip::Strip : P::Strip::Keep;
	};
	strip(_("edition.import_settings.story_effects"), m_profile.storyEffects);
	strip(_("edition.import_settings.global_writes"), m_profile.globalWrites);

	ImGui::Separator();
	if (!m_profileMessage.empty()) ImGui::TextWrapped("%s", m_profileMessage.c_str());
	if (ImGui::Button(_("edition.import_settings.defaults"))) { m_profile = {}; m_profileStoryFlag = 0; }
	ImGui::SameLine();
	if (ImGui::Button(_("edition.import_settings.save"))) {
		// Only the header region changes (ImportProfile::WithProfile); the moveset data is copied byte for byte.
		// An open editor holds its own copy of the header and would revert the profile on its next save.
		if (IsOpenInEditor(m_actionTarget.filename)) m_profileMessage = _("edition.moveset_open_in_editor");
		else try {
			const std::filesystem::path path(m_actionTarget.filename);
			std::ifstream file(path, std::ios::binary);
			const std::vector<Byte> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			file.close();
			const auto updated = ImportProfile::WithProfile(bytes, m_profile);
			if (!FileIO::WriteAtomically(path, updated)) throw std::runtime_error("Cannot write the moveset file");
			m_profileMessage = _("edition.import_settings.saved");
		} catch (const std::exception& error) {
			m_profileMessage = error.what();
		}
	}
}

void Submenu_Edition::ExtractAllAnimations()
{
	m_animExtraction.extractionControl.must_interrupt = false;
	m_animExtraction.extractionControl.current_index = 0;

	std::sort(m_animExtraction.movesets.begin(), m_animExtraction.movesets.end(),
		[](const movesetInfo& a, const movesetInfo& b) -> bool {
			return a.modified < b.modified;
		});

	DEBUG_LOG("Extracting all animations...\n");
	// This runs on a worker thread: an exception escaping it would terminate the program, report it on the current moveset
	auto& control = m_animExtraction.extractionControl;
	try {
		AnimExtractor::ExtractAnimations(m_animExtraction.movesets, control);
	} catch (...) {
		if (control.current_index < control.statuses.size()) {
			control.statuses[control.current_index].status = AnimExtractionStatus_Error;
		}
	}
	m_animExtraction.extracting = false;
}

movesetInfo* Submenu_Edition::Render(LocalStorage& storage)
{
    if (auto result = m_conversionJob.Poll()) {
        m_conversionMessage = result->ok ? std::string(_("edition.convert.saved")) + " " + Helpers::to_utf8(result->output.wstring()) :
            result->cancelled ? std::string(_("edition.convert.cancelled")) : result->error;
        if (result->ok) storage.RefreshMovesets();
    }
    if (m_conversionJob.Busy()) ImGui::TextUnformatted(FileJobStageText(m_conversionJob.Stage()));
    else if (!m_conversionMessage.empty()) ImGui::TextUnformatted(m_conversionMessage.c_str());

	ImGuiExtra::RenderTextbox(_("edition.explanation"));

	ImGui::SeparatorText(_("edition.tools"));
	{
		// Extract all movesets
		if (ImGuiExtra::RenderButtonEnabled(_("edition.extract_animations"), !m_animExtraction.extracting))
		{
			m_animExtraction.extractionControl.statuses = std::vector<s_extractionStatus>(storage.extractedMovesets.size());

			m_animExtraction.movesets.clear();

			// Createe a copy of extractedMovesets so that there's no problem if it changes while we're extracting stuff
			for (auto& item : storage.extractedMovesets) {
				m_animExtraction.movesets.push_back(*item);
			}

			// Extraction starts from the modal's Start button, not when it opens.
			m_animExtraction.ran = false;
			ImGui::OpenPopup("###AnimExtractionPopup");
			m_animExtraction.popup = true;
		}


		ImGui::SameLine();
		if (ImGui::Button(_("edition.open_editor_library")))
		{
			std::wstring path = std::filesystem::current_path();
			path += L"\\" EDITOR_LIB_DIRECTORY;
			DEBUG_LOG("%S\n", path.c_str());
			CreateDirectoryW(path.c_str(), nullptr);
			ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWDEFAULT);
		}

		ImGui::SameLine();
		if (ImGui::Button(_("edition.open_editor_backups")))
		{
			std::wstring path = std::filesystem::current_path();
			path += L"\\" MOVESET_DIRECTORY L"\\" MOVESET_AUTO_BACKUPDIRECTORY;
			DEBUG_LOG("%S\n", path.c_str());
			CreateDirectoryW(path.c_str(), nullptr);
			ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWDEFAULT);
		}

		if (ImGui::Button(_("edition.open_characters_directory")))
		{
            const auto path=std::filesystem::absolute(MovesetFolders::ExtractionDirectory());
            std::filesystem::create_directories(path);
			ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWDEFAULT);
		}

		ImGui::SameLine();
		if (ImGui::Button(_("edition.refresh_movesets")))
		{
			storage.RefreshMovesets();
		}
	}

	if (ImGuiExtra::BeginFittedModal("AnimExtractionPopup", "Extract animations", &m_animExtraction.popup, 40.0f))
	{
		uint32_t total_extracted_count = 0;
		uint32_t total_animations = 0;

		// Fixed height: the modal auto-fits its content, so sizing the table from the remaining space oscillated.
		const ImVec2 tableSize(0, ImGui::GetFrameHeightWithSpacing() * 14);
			
		ImGui::SeparatorText(_("edition.animation_extraction.extraction_status"));
		if (ImGui::BeginTable("AnimExtractionMovesetList", 2, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY
			| ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable, tableSize))
		{
			for (unsigned int i = 0; i < m_animExtraction.movesets.size(); ++i)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				auto& m = m_animExtraction.movesets[i];
				auto& s = m_animExtraction.extractionControl.statuses[i];

				ImGui::TextUnformatted(m.name.c_str());

				ImGui::TableNextColumn();

				switch (s.status)
				{
				case AnimExtractionStatus_NotStarted:
					ImGui::TextUnformatted(_("edition.animation_extraction.not_started"));
					break;
				case AnimExtractionStatus_Skipped:
					ImGui::TextUnformatted(_("edition.animation_extraction.skipped"));
					break;
				case AnimExtractionStatus_SkippedExists:
					ImGui::TextUnformatted(_("edition.animation_extraction.skipped_exists"));
					break;
				case AnimExtractionStatus_Error:
					ImGui::TextUnformatted(_("edition.animation_extraction.error"));
					break;
				case AnimExtractionStatus_Started:
					total_extracted_count += s.current_animation;
					total_animations += s.total_animation_count;
					ImGui::TextUnformatted(std::format("{}/{}", s.current_animation, s.total_animation_count).c_str());
					break;
				case AnimExtractionStatus_Finished:
					total_extracted_count += s.current_animation;
					total_animations += s.total_animation_count;
					ImGui::TextUnformatted(std::to_string(s.current_animation).c_str());
					break;
				}
			}

			ImGui::EndTable();
		}


		ImGui::TextUnformatted(std::format("{}/{}", total_extracted_count, total_animations).c_str());

		if (!m_animExtraction.extracting) {
			if (m_animExtraction.ran && !m_animExtraction.extractionControl.must_interrupt)
				ImGui::TextColored(ImVec4(0, 1.0f, 0, 1), "%s", _("edition.extract_animations_finished"));
			if (ImGuiExtra::RenderButtonEnabled(_("edition.t8_disk.start"), !m_animExtraction.movesets.empty())) {
				if (m_animExtraction.started) m_animExtraction.thread.join();
				m_animExtraction.extractionControl.statuses = std::vector<s_extractionStatus>(m_animExtraction.movesets.size());
				m_animExtraction.started = true;
				m_animExtraction.extracting = true;
				m_animExtraction.ran = true;
				m_animExtraction.thread = std::thread(&Submenu_Edition::ExtractAllAnimations, this);
			}
			ImGui::SameLine();
			if (ImGui::Button(_("close")))
			{
				ImGui::CloseCurrentPopup();
				m_animExtraction.popup = false;
				m_animExtraction.extractionControl.must_interrupt = true;
			}
		}
		else {
			if (ImGuiExtra::RenderButtonEnabled(_("edition.animation_extraction.interrupt"), !m_animExtraction.extractionControl.must_interrupt)) {
				m_animExtraction.extractionControl.must_interrupt = true;
			}
		}

		ImGui::EndPopup();
	}
	


	ImGui::SeparatorText(_("edition.select_moveset"));

	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##movesetSearch", _("edition.search_hint"), m_search, sizeof(m_search));
	const std::wstring searchNeedle = LowerForSearch(m_search);

	movesetInfo* movesetToLoad = nullptr;

	auto availableSpace = ImGui::GetContentRegionAvail();
	ImVec2 tableSize = ImVec2(0, availableSpace.y);

#ifdef BUILD_TYPE_DEBUG
	const unsigned int rowCount = 11;
	// +1 row for original extraction date
#else
	const unsigned int rowCount = 10;
#endif
	if (ImGui::BeginTable("MovesetEditionList", rowCount, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY
		| ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable, tableSize))
	{
		ImGui::TableSetupColumn(_("moveset.name"), ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch, 0.0f, MovesetListSort::Name);
		ImGui::TableSetupColumn(_("moveset.game"), 0, 0.0f, MovesetListSort::Game);
		ImGui::TableSetupColumn(_("moveset.target_character"), ImGuiTableColumnFlags_NoSort);
		ImGui::TableSetupColumn(_("moveset.date"), ImGuiTableColumnFlags_NoSort);
#ifdef BUILD_TYPE_DEBUG
		ImGui::TableSetupColumn(_("moveset.extraction_date"), ImGuiTableColumnFlags_NoSort);
#endif
		ImGui::TableSetupColumn(_("moveset.size"), ImGuiTableColumnFlags_NoSort);
		ImGui::TableSetupColumn(_("moveset.convert"), ImGuiTableColumnFlags_NoSort);
		ImGui::TableSetupColumn(_("moveset.import_settings"), ImGuiTableColumnFlags_NoSort);
		ImGui::TableSetupColumn(_("moveset.duplicate"), ImGuiTableColumnFlags_NoSort);
		ImGui::TableSetupColumn(_("moveset.rename"), ImGuiTableColumnFlags_NoSort);
		ImGui::TableSetupColumn(_("moveset.edit"), ImGuiTableColumnFlags_NoSort);
		ImGui::TableHeadersRow();

		ImGui::PushID(&storage);

		// Movesets are guaranteed not to be freed until after this loop
		for (auto& group : MovesetListSort::GroupedTableView(storage.extractedMovesets))
		{
		if (!searchNeedle.empty()) {
			// Games without a matching moveset are hidden entirely, header included
			std::erase_if(group.movesets, [&](const movesetInfo* m) { return !MatchesSearch(*m, searchNeedle); });
			if (group.movesets.empty()) continue;
		}
		if (!MovesetListSort::GroupHeader(group, group.game == "INVALID" ? _("moveset.invalid") : group.game.c_str())) continue;
		for (movesetInfo* moveset : group.movesets)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			MovesetListSort::RowHover hover;
			hover.Begin();

			if (moveset->color != 0) {
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, moveset->color);
			}

			ImGui::TextUnformatted(moveset->name.c_str());
			if (!moveset->notes.empty()) { ImGui::SameLine(); ImGuiExtra::HelpMarker(moveset->notes.c_str(), true); }

			ImGui::TableNextColumn();
			if (moveset->origin == "INVALID") {
				// Badly formatted file. Display it, but mention it is invalid
				ImGui::TextUnformatted(_("moveset.invalid"));
			}
			else {
				ImGui::TextUnformatted(moveset->origin.c_str());

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(moveset->target_character.c_str());

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(moveset->date_str.c_str());

#ifdef BUILD_TYPE_DEBUG
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(moveset->extraction_date_str.c_str());
#endif

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(moveset->sizeStr.c_str());

				ImGui::PushID(moveset->filename.c_str());

				ImGui::TableNextColumn();
				if (ImGuiExtra::RenderButtonEnabled(_("moveset.convert"), HasConversionTarget(moveset->gameId))) {
					m_actionTarget = *moveset;
					m_conversionPopup = true;
				}

				ImGui::TableNextColumn();
				if (ImGui::Button(_("moveset.import_settings"))) {
					m_actionTarget = *moveset;
					m_profileMessage.clear();
					try {
						std::ifstream file(std::filesystem::path(moveset->filename), std::ios::binary);
						const std::vector<Byte> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
						m_profile = ImportProfile::Read(bytes);
					} catch (const std::exception& error) { m_profile = {}; m_profileMessage = error.what(); }
					m_profileStoryFlag = m_profile.storyFlag == ImportProfile::StoryFlagGameDecides ? 0 : m_profile.storyFlag;
					m_profilePopup = true;
				}

				ImGui::TableNextColumn();
				if (ImGui::Button(_("moveset.duplicate"))) {
					DuplicateMoveset(moveset->filename);
				}

				ImGui::TableNextColumn();
				if (ImGui::Button(_("moveset.rename"))) {
					m_actionTarget = *moveset;
					CopyUtf8Truncated(m_newName, sizeof(m_newName), moveset->name);
					m_renamePopup = true;
					currentErr = RenameErrcode_NoErr;
				}

				ImGui::TableNextColumn();
				ImGui::PushStyleColor(ImGuiCol_Button, FORM_INPUT_HEX);
				if (ImGuiExtra::RenderButtonEnabled(_("moveset.edit"), moveset->editable)) {
					movesetToLoad = moveset;
				}
				ImGui::PopStyleColor();
				ImGui::PopID();
			}
			hover.End();
		}
		}
		ImGui::PopID();

		ImGui::EndTable();
	}

	if (m_renamePopup) {
		ImGui::OpenPopup("###EditionRenamePopup");
	}
	else if (m_conversionPopup) {
		ImGui::OpenPopup("###EditionConvertPopup");
	}
	else if (m_profilePopup) {
		ImGui::OpenPopup("###EditionImportProfilePopup");
	}

	if (ImGuiExtra::BeginFittedModal("EditionRenamePopup", "Rename", &m_renamePopup))
	{
		RenderRenamePopup();
		ImGui::EndPopup();
	}
	else if (ImGuiExtra::BeginFittedModal("EditionConvertPopup", "Convert", &m_conversionPopup))
	{
		RenderConversionPopup();
		ImGui::EndPopup();
	}
	else if (ImGuiExtra::BeginFittedModal("EditionImportProfilePopup", _("edition.import_settings.title"), &m_profilePopup))
	{
		RenderImportProfilePopup();
		ImGui::EndPopup();
	}

	return movesetToLoad;
}

Submenu_Edition::~Submenu_Edition()
{
	if (m_animExtraction.started) {
		m_animExtraction.thread.join();
	}
}
