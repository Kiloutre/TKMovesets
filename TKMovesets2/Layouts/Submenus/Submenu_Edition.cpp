#include <ImGui.h>
#include <filesystem>
#include <system_error>

#include "Submenu_Edition.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "helpers.hpp"
#include "MovesetConverters/MovesetFileConverter.hpp"

// -- Static helpers -- //

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

	std::wstring w_newName = Helpers::to_unicode(newName);
	std::wstring new_full_filename = full_filename.substr(0, full_filename.find_last_of(L"/\\") + 1)
									+ w_newName + (L"" MOVESET_FILENAME_EXTENSION);

	if (std::string(newName).find_first_of("/\\<>:\"|?*") != -1) {
		return RenameErrcode_InvalidName;
	}

	if (Helpers::fileExists(new_full_filename.c_str()) && full_filename != new_full_filename) {
		return RenameErrcode_AlreadyExists;
	}

	try {
		std::filesystem::rename(full_filename.c_str(), new_full_filename.c_str());
	} catch (std::exception& _) {
        DEBUG_ERR("Failed to rename moveset");
		(void)_;
		// Todo: maybe an error popup of some kind
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
		currentErr = RenameMoveset(m_actionTarget.filename, m_newName);
		if (currentErr == RenameErrcode_NoErr) {
			ImGui::CloseCurrentPopup();
			m_renamePopup = false;
			return;
		}
	}

	switch (currentErr)
	{
	case RenameErrcode_EmptyName:
		ImGui::TextColored(ImVec4(1, 0, 0.2f, 1), _("edition.rename.empty_name"));
		break;
	case RenameErrcode_AlreadyExists:
		ImGui::TextColored(ImVec4(1, 0, 0.2f, 1), _("edition.rename.already_exists"));
		break;
	case RenameErrcode_RenameErr:
		ImGui::TextColored(ImVec4(1, 0, 0.2f, 1), _("edition.rename.rename_err"));
		break;
	case RenameErrcode_InvalidName:
		ImGui::TextColored(ImVec4(1, 0, 0.2f, 1), _("edition.rename.invalid_name"));
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

	ImGui::SeparatorText(_("edition.convert.format_target"));

	auto gameListCount = Games::GetGamesCount();
	const GameInfo* games = Games::GetGameInfoFromIndex(0);

	ImGui::PushID(games);
	for (unsigned int i = 0; i < gameListCount; ++i)
	{
		if (games->supportedImports.contains(m_actionTarget.gameId)){
			if (ImGui::Button(games->name)) {
				ConvertMoveset(m_actionTarget, (GameId_)games->gameId);
				ImGui::CloseCurrentPopup();
				m_conversionPopup = false;
				ImGui::PopID();
				return;
			}
		}
		++games;
	}
	ImGui::PopID();

}

movesetInfo* Submenu_Edition::Render(LocalStorage& storage)
{
	ImGuiExtra::RenderTextbox(_("edition.explanation"));
	ImGui::SeparatorText(_("edition.select_moveset"));

	movesetInfo* movesetToLoad = nullptr;

	auto availableSpace = ImGui::GetContentRegionAvail();
	ImVec2 tableSize = ImVec2(0, availableSpace.y);

	if (ImGui::BeginTable("MovesetEditionList", 9, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY
		| ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable, tableSize))
	{
		ImGui::TableSetupColumn("##", 0, 5.0f);
		ImGui::TableSetupColumn(_("moveset.origin"));
		ImGui::TableSetupColumn(_("moveset.target_character"));
		ImGui::TableSetupColumn(_("moveset.date"));
		ImGui::TableSetupColumn(_("moveset.size"));
		ImGui::TableSetupColumn(_("moveset.convert"));
		ImGui::TableSetupColumn(_("moveset.duplicate"));
		ImGui::TableSetupColumn(_("moveset.rename"));
		ImGui::TableSetupColumn(_("moveset.edit"));
		ImGui::TableHeadersRow();

		ImGui::PushID(&storage);

		// drawList & windowPos are used to display a different row bg
		ImDrawList* drawlist = ImGui::GetWindowDrawList();
		auto windowPos = ImGui::GetWindowPos();

		// Yes, we don't use an iterator here because the vector might actually change size mid-iteration
		for (size_t i = 0; i < storage.extractedMovesets.size(); ++i)
		{
			// moveset is guaranteed not to be freed until after this loop
			movesetInfo* moveset = storage.extractedMovesets[i];

			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			if (moveset->color != 0)
			{
				// Draw BG
				ImVec2 drawStart = windowPos + ImGui::GetCursorPos();
				drawStart.y -= ImGui::GetScrollY();
				ImVec2 drawArea = ImVec2(availableSpace.x, ImGui::GetFrameHeight());
				drawlist->AddRectFilled(drawStart, drawStart + drawArea, moveset->color);
			}

			ImGui::TextUnformatted(moveset->name.c_str());

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

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(moveset->sizeStr.c_str());

				ImGui::PushID(moveset->filename.c_str());

				ImGui::TableNextColumn();
				if (ImGui::Button(_("moveset.convert"))) {
					m_actionTarget = *moveset;
					m_conversionPopup = true;
				}

				ImGui::TableNextColumn();
				if (ImGui::Button(_("moveset.duplicate"))) {
					DuplicateMoveset(moveset->filename);
				}

				ImGui::TableNextColumn();
				if (ImGui::Button(_("moveset.rename"))) {
					m_actionTarget = *moveset;
					strcpy_s(m_newName, sizeof(m_newName), moveset->name.c_str());
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
		}
		ImGui::PopID();

		ImGui::EndTable();
	}

	if (m_renamePopup) {
		ImGui::OpenPopup("EditionRenamePopup");
		ImGui::SetNextWindowSizeConstraints(ImVec2(500, 200), ImVec2(1920, 1080));
	}
	else if (m_conversionPopup) {
		ImGui::OpenPopup("EditionConvertPopup");
		ImGui::SetNextWindowSizeConstraints(ImVec2(500, 200), ImVec2(1920, 1080));
	}

	if (ImGui::BeginPopupModal("EditionRenamePopup", &m_renamePopup))
	{
		RenderRenamePopup();
		ImGui::EndPopup();
	}
	else if (ImGui::BeginPopupModal("EditionConvertPopup", &m_conversionPopup))
	{
		RenderConversionPopup();
		ImGui::EndPopup();
	}

	return movesetToLoad;
}
