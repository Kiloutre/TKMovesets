#include <ImGui.h>
#include <format>

#include "Submenu_Edition.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "helpers.hpp"

// -- Public methods -- //

movesetInfo* Submenu_Edition::Render(LocalStorage& storage)
{
	ImGuiExtra::RenderTextbox(_("edition.explanation"));

	movesetInfo* movesetToLoad = nullptr;

	ImGui::SeparatorText(_("edition.select_moveset"));
	if (ImGui::BeginTable("MovesetEditionList", 6, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY
		| ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable, ImVec2(0, ImGui::GetContentRegionAvail().y)))
	{
		ImGui::TableSetupColumn("##", 0, 5.0f);
		ImGui::TableSetupColumn(_("moveset.origin"));
		ImGui::TableSetupColumn(_("moveset.target_character"));
		ImGui::TableSetupColumn(_("moveset.date"));
		ImGui::TableSetupColumn(_("moveset.size"));
		ImGui::TableSetupColumn(_("moveset.edit"));
		ImGui::TableHeadersRow();

		// Yes, we don't use an iterator here because the vector might actually change size mid-iteration
		ImGui::PushID(&storage);
		for (size_t i = 0; i < storage.extractedMovesets.size(); ++i)
		{
			// moveset is guaranteed not to be freed until after this loop
			movesetInfo* moveset = storage.extractedMovesets[i];

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
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
				std::string sizeString = std::format("{:.2f} {}", moveset->size, _("moveset.size_mb"));
				ImGui::TextUnformatted(sizeString.c_str());

				ImGui::TableNextColumn();
				ImGui::PushID(moveset->filename.c_str());

				if (ImGuiExtra::RenderButtonEnabled(_("moveset.edit"), moveset->editable)) {
					movesetToLoad = moveset;
				}
				ImGui::PopID();
			}
		}
		ImGui::PopID();

		ImGui::EndTable();
	}

	return movesetToLoad;
}
