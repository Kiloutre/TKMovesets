#include <ImGui.h>

#include "Submenu_Addresses.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "Helpers.hpp"
#include "FileIO.hpp"
#include "Games.hpp"
#include "constants.h"

#include <format>
#include <shobjidl.h>

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

// Translation strings hold a {} placeholder; they are only known at run time.
template<class T> std::string Translated(const char* text, T value) { return std::vformat(text, std::make_format_args(value)); }

}

Submenu_Addresses::Submenu_Addresses()
{
    // Only retail Tekken 8 has exe byte patterns so far; the list is here for the games that get them later.
    for (unsigned i = 0; i < Games::GetGamesCount(); ++i) {
        const auto* game = Games::GetGameInfoFromIndex(i);
        if (game && game->gameId == GameId_T8 && game->minorVersion == GameVersions::T8::RETAIL)
            m_targets.push_back({ game->name, game->dataString });
    }
}

Submenu_Addresses::~Submenu_Addresses()
{
    if (m_thread.joinable()) m_thread.join();
}

void Submenu_Addresses::Start(const GameAddressesFile& addresses)
{
    if (m_thread.joinable()) m_thread.join();
    m_rows.clear(); m_status.clear(); m_error.clear();
    const std::string gameKey = m_targets[m_target].gameKey;
    auto patterns = addresses.GetStringsWithPrefix(gameKey, "sig_");
    if (patterns.empty()) { m_error = _("addresses.no_patterns"); return; }
    // m_executable is UTF-8 (file dialog and ImGui input): decode it, do not widen it byte by byte.
    std::filesystem::path exe;
    try { exe = Helpers::to_unicode(m_executable); }
    catch (const std::exception&) { m_error = _("addresses.invalid_path"); return; }
    m_running = true;
    m_thread = std::thread([this, exe, patterns = std::move(patterns)] {
        std::optional<ExeSignatures::ScanReport> report; std::string error;
        try { report = ExeSignatures::Scan(ExeSignatures::ReadImage(exe), patterns); }
        catch (const std::exception& e) { error = e.what(); }
        std::lock_guard lock(m_mutex);
        if (report) m_pending = std::move(report);
        else { m_pendingError = error; m_hasPendingError = true; }
        m_running = false;
    });
}

void Submenu_Addresses::Apply(GameAddressesFile& addresses, const ExeSignatures::ScanReport& report)
{
    using ExeSignatures::Outcome;
    const auto result = ExeSignatures::ApplyScan(addresses, m_targets[m_target].gameKey, report, GAME_ADDRESSES_FILE, true);
    m_scannedTimestamp = result.peTimestamp;
    m_rows = result.rows;
    if (result.writeFailed) { m_error = _("addresses.write_failed"); return; }
    if (result.updated) m_status = Translated(_("addresses.updated"), result.updated);
    else m_status = result.failed ? _("addresses.nothing_changed") : _("addresses.up_to_date");
    if (result.failed) m_status += " " + Translated(_("addresses.some_failed"), result.failed);
}

void Submenu_Addresses::RenderResults()
{
    if (m_rows.empty()) return;
    ImGui::SeparatorText(_("addresses.results"));
    ImGui::Text(_("addresses.exe_build"), m_scannedTimestamp);
    const auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
    if (!ImGui::BeginTable("##addressResults", 5, flags)) return;
    ImGui::TableSetupColumn(_("addresses.col_key"));
    ImGui::TableSetupColumn(_("addresses.col_before"));
    ImGui::TableSetupColumn(_("addresses.col_found"));
    ImGui::TableSetupColumn(_("addresses.col_patterns"));
    ImGui::TableSetupColumn(_("addresses.col_result"));
    ImGui::TableHeadersRow();
    for (const auto& row : m_rows) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextUnformatted(row.scan.key.c_str());
        ImGui::TableNextColumn(); ImGui::TextUnformatted(row.before ? ExeSignatures::Hex(*row.before).c_str() : "-");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(row.scan.status == ExeSignatures::KeyStatus::Found ? ExeSignatures::Hex(row.scan.value).c_str() : "-");
        ImGui::TableNextColumn();
        size_t found = 0;
        for (const auto& p : row.scan.patterns) found += p.status == ExeSignatures::PatternStatus::Found;
        ImGui::Text("%zu / %zu", found, row.scan.patterns.size());
        if (ImGui::IsItemHovered()) {
            std::string tip;
            for (const auto& p : row.scan.patterns) {
                tip += p.name + ": ";
                switch (p.status) {
                case ExeSignatures::PatternStatus::Found: tip += ExeSignatures::Hex(p.value); break;
                case ExeSignatures::PatternStatus::NotFound: tip += _("addresses.pattern_not_found"); break;
                case ExeSignatures::PatternStatus::Ambiguous: tip += _("addresses.pattern_ambiguous"); break;
                case ExeSignatures::PatternStatus::Invalid: tip += p.detail; break;
                }
                tip += "\n";
            }
            ImGui::SetTooltip("%s", tip.c_str());
        }
        ImGui::TableNextColumn();
        switch (row.outcome) {
        case ExeSignatures::Outcome::Updated: ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", _("addresses.result_updated")); break;
        case ExeSignatures::Outcome::Unchanged: ImGui::TextUnformatted(_("addresses.result_unchanged")); break;
        case ExeSignatures::Outcome::NotFound: ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), "%s", _("addresses.result_not_found")); break;
        case ExeSignatures::Outcome::Conflict: ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", _("addresses.result_conflict")); break;
        case ExeSignatures::Outcome::NotInIni: ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), "%s", _("addresses.result_not_in_ini")); break;
        }
    }
    ImGui::EndTable();
}

void Submenu_Addresses::Render(GameAddressesFile& addresses, bool workersBusy)
{
    // A finished scan is applied here, on the UI thread, where the address data is otherwise only read. Never while an
    // extraction/import runs (it may have started after the scan did): the report stays pending until they are idle.
    bool waiting = false;
    {
        std::optional<ExeSignatures::ScanReport> report;
        {
            std::lock_guard lock(m_mutex);
            if (!workersBusy) report.swap(m_pending);
            else waiting = m_pending.has_value();
            if (m_hasPendingError) { m_error = m_pendingError; m_hasPendingError = false; }
        }
        if (report) Apply(addresses, *report);
    }

    ImGuiExtra::RenderTextbox(_("addresses.explanation"));
    ImGui::SeparatorText(_("addresses.title"));
    if (m_targets.empty()) return;

    const bool running = m_running;
    {
        ImGuiExtra::DisableBlockIf __(running);
        ImGui::TextUnformatted(_("addresses.game"));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("##addressGame", m_targets[m_target].name)) {
            for (int i = 0; i < static_cast<int>(m_targets.size()); ++i)
                if (ImGui::Selectable(m_targets[i].name, i == m_target)) { m_target = i; m_rows.clear(); m_status.clear(); m_error.clear(); }
            ImGui::EndCombo();
        }

        if (!m_executableDetected) {
            m_executableDetected = true;
            if (auto exe = ExeSignatures::FindT8Executable()) snprintf(m_executable, sizeof(m_executable), "%s", Helpers::to_utf8(exe->wstring()).c_str());
        }
        ImGui::TextUnformatted(_("addresses.executable"));
        ImGui::SetNextItemWidth(-260);
        ImGui::InputTextWithHint("##addressExe", _("addresses.executable_hint"), m_executable, sizeof(m_executable));
        ImGui::SameLine();
        if (ImGui::Button(_("addresses.browse"))) BrowseExecutable(m_executable, sizeof(m_executable));
        ImGui::SameLine();
        if (ImGui::Button(_("addresses.detect"))) {
            if (auto exe = ExeSignatures::FindT8Executable()) snprintf(m_executable, sizeof(m_executable), "%s", Helpers::to_utf8(exe->wstring()).c_str());
            else m_error = _("addresses.not_detected");
        }
    }

    ImGui::Spacing();
    const bool canRun = !running && !workersBusy && m_executable[0];
    if (ImGuiExtra::RenderButtonEnabled(_("addresses.update"), canRun)) Start(addresses);
    if (running) { ImGui::SameLine(); ImGui::TextUnformatted(_("addresses.scanning")); }
    else if (workersBusy || waiting) { ImGui::SameLine(); ImGui::TextUnformatted(_("addresses.busy")); } // also: a finished scan waits

    if (!m_error.empty()) ImGuiExtra_TextboxError(m_error.c_str());
    if (!m_status.empty()) ImGui::TextWrapped("%s", m_status.c_str());
    RenderResults();
}
