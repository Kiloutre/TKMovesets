#include "T8DiskExtraction.hpp"
#include "Extractor_t8/RetailDiskMoveset.hpp"
#include "Extractor_t8/RetailGameData.hpp"
#include "Extractor_t7/T7DiskMoveset.hpp"
#include "AddressScanner/ExeSignatures.hpp"
#include <cctype>
#include "GameStaticData.hpp"
#include "Localization.hpp"
#include "MovesetFolders.hpp"
#include "Helpers.hpp"
#include "imgui_extras.hpp"
#include <ImGui.h>
#include <algorithm>
#include <format>
#include <stdexcept>
#include <Windows.h>
#include <shobjidl.h>

void T8DiskExtraction::Build(std::filesystem::path tkdata, uint8_t categories)
{
    // One file each next to the memory exports; interrupt is checked between movesets, each file is written whole or not at all.
    try {
        const auto directory = std::filesystem::absolute(MovesetFolders::ExtractionDirectory());
        std::filesystem::create_directories(directory);
        if (m_game == Game::T7) {
            const auto common = T7Disk::ReadTkdataFile(tkdata, "mothead\\mask\\common.arc");
            for (auto& item : m_items) {
                if (m_interrupt) break;
                if (!item->selected) continue;
                item->status = AnimExtractionStatus_Started;
                try {
                    const auto report = T7Disk::WriteMovesetFile(tkdata, item->code, common, directory);
                    item->animations = static_cast<uint32_t>(report.animations);
                    item->moves = static_cast<uint32_t>(report.moves);
                    item->status = report.existed ? AnimExtractionStatus_SkippedExists : AnimExtractionStatus_Finished;
                    if (!report.existed) m_refresh = true;
                } catch (const std::exception& error) {
                    item->error = error.what();
                    item->status = AnimExtractionStatus_Error;
                }
            }
            m_running = false;
            return;
        }
        const RetailT8GameData::Anmbin common(RetailT8GameData::ReadTkdataFile(tkdata, "mothead/bin/com.anmbin"));
        for (auto& item : m_items) {
            if (m_interrupt) break;
            if (!item->selected) continue;
            const auto path = directory / Helpers::string_to_wstring(RetailT8Disk::FileName(item->code));
            if (std::filesystem::exists(path)) { item->status = AnimExtractionStatus_SkippedExists; continue; }
            item->status = AnimExtractionStatus_Started;
            try {
                const auto report = RetailT8Disk::WriteMovesetFile(tkdata, item->code, common, path, categories);
                item->animations = static_cast<uint32_t>(report.animations);
                item->moves = static_cast<uint32_t>(report.moves);
                item->status = AnimExtractionStatus_Finished;
                m_refresh = true;
            } catch (const std::exception& error) {
                item->error = error.what();
                item->status = AnimExtractionStatus_Error;
            }
        }
    } catch (const std::exception& error) {
        for (auto& item : m_items)
            if (item->selected && item->status == AnimExtractionStatus_NotStarted) { item->error = error.what(); item->status = AnimExtractionStatus_Error; }
    }
    m_running = false;
}

namespace {
// Windows file picker restricted to .exe files; leaves `buffer` untouched when cancelled.
void BrowseExecutable(char* buffer, size_t capacity)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return;
    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        const COMDLG_FILTERSPEC filters[] = { { L"Executable (*.exe)", L"*.exe" } };
        dialog->SetFileTypes(1, filters);
        // FOS_NOCHANGEDIR: game_addresses.ini and other files are opened relative to the working directory
        dialog->SetOptions(FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
        if (SUCCEEDED(dialog->Show(GetActiveWindow()))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    snprintf(buffer, capacity, "%s", Helpers::to_utf8(path).c_str());
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dialog->Release();
    }
    if (SUCCEEDED(initialized)) CoUninitialize();
}
}

void T8DiskExtraction::LoadCharacters()
{
    m_items.clear(); m_error.clear(); m_tkdata.clear(); m_started = false;
    if (!m_executable[0]) return;
    try {
        const std::filesystem::path executable(Helpers::to_unicode(m_executable));
        if (!std::filesystem::is_regular_file(executable)) throw std::runtime_error(std::format("File not found: {}", m_executable));
        const auto tkdata = m_game == Game::T7 ? T7Disk::TkdataPath(executable) : RetailT8GameData::TkdataPath(executable);
        if (m_game == Game::T7) {
            for (const auto& [code, name] : T7Disk::CharacterNames(tkdata)) {
                auto item = std::make_unique<Item>();
                item->code = code; item->name = name;
                m_items.push_back(std::move(item));
            }
        } else {
            // every character moveset the archive holds (it decides which exist), named from the names file when it
            // knows the code; fighters it doesn't name (unreleased/test ones) are listed by their code
            const auto names = GameStaticData::LoadOptional("t8-retail");
            for (const auto& fighter : RetailT8GameData::TkdataRoster(tkdata)) {
                const auto character = names.FindCharacter(fighter.code);
                auto item = std::make_unique<Item>();
                item->code = fighter.code;
                item->name = character && !character->name.empty() ? character->name : fighter.code;
                m_items.push_back(std::move(item));
            }
            if (m_items.empty()) throw std::runtime_error("No character moveset was found in " + Helpers::to_utf8(tkdata.wstring()));
            std::sort(m_items.begin(), m_items.end(), [](const auto& a, const auto& b) { return a->name < b->name; });
        }
        m_tkdata = tkdata;
    } catch (const std::exception& error) { m_items.clear(); m_error = error.what(); }
}

void T8DiskExtraction::Render(Game gameKind, const std::filesystem::path& gameExecutable, bool enabled, LocalStorage& storage)
{
    if (m_refresh.exchange(false)) storage.RefreshMovesets();
    const auto attachedExecutable = gameExecutable.empty() ? std::string() : Helpers::to_utf8(gameExecutable.wstring());
    if (ImGuiExtra::RenderButtonEnabled(_(gameKind == Game::T7 ? "edition.t7_disk.button" : "edition.t8_disk.button"), enabled && !m_running)) {
        // The running game's exe wins; otherwise the last path typed for this game stays; otherwise the Steam install.
        if (gameKind != m_game) m_executable[0] = 0;
        m_game = gameKind; m_interrupt = false;
        if (!attachedExecutable.empty()) snprintf(m_executable, sizeof(m_executable), "%s", attachedExecutable.c_str());
        else if (!m_executable[0]) {
            const auto installed = gameKind == Game::T7 ? ExeSignatures::FindInstalledT7Executable() : ExeSignatures::FindInstalledT8Executable();
            if (installed) snprintf(m_executable, sizeof(m_executable), "%s", Helpers::to_utf8(installed->wstring()).c_str());
        }
        LoadCharacters();
        ImGui::OpenPopup("###T8DiskExtractionPopup");
        m_popup = true;
    }

    if (!ImGuiExtra::BeginFittedModal("T8DiskExtractionPopup", _(m_game == Game::T7 ? "edition.t7_disk.title" : "edition.t8_disk.title"), &m_popup, 40.0f)) {
        if (m_running) m_interrupt = true; // closed while running
        return;
    }
    ImGuiExtra::RenderTextbox(_("edition.t8_disk.explanation"));
    // Targeted exe: typed, pasted or browsed; filled in when the game gets attached while the field is empty.
    ImGui::SeparatorText(_(m_game == Game::T7 ? "edition.t7_disk.executable" : "edition.t8_disk.executable"));
    ImGui::BeginDisabled(m_running);
    if (!m_executable[0] && !attachedExecutable.empty()) {
        snprintf(m_executable, sizeof(m_executable), "%s", attachedExecutable.c_str());
        LoadCharacters();
    }
    ImGui::SetNextItemWidth(-ImGui::CalcTextSize(_("settings.folders.browse")).x - ImGui::GetStyle().FramePadding.x * 2 - ImGui::GetStyle().ItemSpacing.x);
    ImGui::InputTextWithHint("##executable", _("edition.t8_disk.executable_hint"), m_executable, sizeof(m_executable));
    if (ImGui::IsItemDeactivatedAfterEdit()) LoadCharacters();
    ImGui::SameLine();
    if (ImGui::Button(_("settings.folders.browse"))) {
        const std::string previous = m_executable;
        BrowseExecutable(m_executable, sizeof(m_executable));
        if (previous != m_executable) LoadCharacters();
    }
    ImGui::EndDisabled();
    if (!m_error.empty()) ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", m_error.c_str());
    if (m_game == Game::T8Retail) {
        // Animation categories carried by every file (same choice and defaults as the export settings). T7 files carry every MOTA.
        ImGui::SeparatorText(_("extraction.panm.title"));
        ImGui::BeginDisabled(m_running);
        constexpr const char* labels[] = {"extraction.panm.body", "extraction.panm.hands", "extraction.panm.face", "extraction.panm.swing", "extraction.panm.camera", "extraction.panm.extra"};
        for (unsigned i = 0; i < 6; ++i) { ImGui::Checkbox(_(labels[i]), &m_categories[i]); if (!(i & 1)) ImGui::SameLine(); }
        ImGui::EndDisabled();
    }
    uint32_t finished = 0, selected = 0;
    ImGui::SeparatorText(_("edition.t8_disk.characters"));
    ImGui::BeginDisabled(m_running);
    if (ImGui::SmallButton(_("extraction.panm.all"))) for (auto& item : m_items) item->selected = true;
    ImGui::SameLine();
    if (ImGui::SmallButton(_("edition.t8_disk.select_none"))) for (auto& item : m_items) item->selected = false;
    ImGui::EndDisabled();
    if (ImGui::BeginTable("T8DiskMovesets", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable,
        ImVec2(0, ImGui::GetFrameHeightWithSpacing() * 14))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(_("edition.t8_disk.column_name"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(_("edition.t8_disk.column_code"), ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(_("edition.t8_disk.column_status"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (const auto& item : m_items) {
            ImGui::TableNextRow(); ImGui::TableNextColumn();
            ImGui::PushID(item.get());
            ImGui::BeginDisabled(m_running);
            ImGui::Checkbox(item->name.c_str(), &item->selected);
            ImGui::EndDisabled();
            ImGui::PopID();
            if (item->selected) ++selected;
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(item->code.c_str());
            ImGui::TableNextColumn();
            if (!item->selected && item->status == AnimExtractionStatus_NotStarted) { ImGui::TextDisabled("-"); continue; }
            switch (item->status.load()) {
            case AnimExtractionStatus_NotStarted: ImGui::TextUnformatted(_("edition.animation_extraction.not_started")); break;
            case AnimExtractionStatus_SkippedExists: ImGui::TextUnformatted(_("edition.animation_extraction.skipped_exists")); ++finished; break;
            case AnimExtractionStatus_Error:
                ImGui::TextUnformatted(_("edition.animation_extraction.error"));
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", item->error.c_str());
                ++finished; break;
            case AnimExtractionStatus_Started: ImGui::TextUnformatted(_("edition.t8_disk.building")); break;
            case AnimExtractionStatus_Finished: {
                const auto moves = item->moves.load(), animations = item->animations.load();
                ImGui::TextUnformatted(std::vformat(_("edition.t8_disk.built"), std::make_format_args(moves, animations)).c_str());
                ++finished; break;
            }
            default: break;
            }
        }
        ImGui::EndTable();
    }
    ImGui::TextUnformatted(std::format("{}/{}", finished, selected).c_str());
    if (m_running) {
        if (ImGuiExtra::RenderButtonEnabled(_("edition.animation_extraction.interrupt"), !m_interrupt)) m_interrupt = true;
    } else {
        if (m_started && selected && finished == selected) ImGui::TextColored(ImVec4(0, 1.0f, 0, 1), "%s", _("edition.t8_disk.finished"));
        if (ImGuiExtra::RenderButtonEnabled(_("edition.t8_disk.start"), !m_tkdata.empty() && selected != 0)) {
            if (m_thread.joinable()) m_thread.join();
            for (auto& item : m_items)
                if (item->status != AnimExtractionStatus_Finished && item->status != AnimExtractionStatus_SkippedExists) {
                    item->status = AnimExtractionStatus_NotStarted; item->error.clear();
                }
            uint8_t categories = 0;
            for (unsigned i = 0; i < 6; ++i) if (m_categories[i]) categories |= uint8_t(1u << i);
            m_interrupt = false; m_running = true; m_started = true;
            m_thread = std::thread(&T8DiskExtraction::Build, this, m_tkdata, categories);
        }
        ImGui::SameLine();
        if (ImGui::Button(_("close"))) { ImGui::CloseCurrentPopup(); m_popup = false; }
    }
    ImGui::EndPopup();
}

T8DiskExtraction::~T8DiskExtraction()
{
    m_interrupt = true;
    if (m_thread.joinable()) m_thread.join();
}
