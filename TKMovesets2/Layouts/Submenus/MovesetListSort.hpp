#pragma once
#include <windows.h>
#include "LocalStorage.hpp"
#include <ImGui.h>
#include <algorithm>
#include <cwctype>
#include <span>

namespace MovesetListSort {
inline constexpr ImGuiID Name = 1, Game = 2;
inline int Compare(std::wstring_view a, std::wstring_view b) {
    if (a.empty() || b.empty()) return a.empty() ? (b.empty() ? 0 : -1) : 1;
    const auto order = CompareStringEx(LOCALE_NAME_INVARIANT,NORM_IGNORECASE | SORT_STRINGSORT,
        a.data(),static_cast<int>(a.size()),b.data(),static_cast<int>(b.size()),nullptr,nullptr,0);
    return order == CSTR_LESS_THAN ? -1 : order == CSTR_GREATER_THAN ? 1 : 0;
}
// Sort a view, never the shared published list. Each menu retains its own table
// sort settings, while new/renamed/deleted files are reflected on every frame.
inline std::vector<movesetInfo*> View(std::span<movesetInfo* const> source, const ImGuiTableSortSpecs* specs) {
    struct Row { movesetInfo* moveset; std::wstring name, game; };
    std::vector<Row> rows;
    rows.reserve(source.size());
    for (auto* moveset : source) rows.push_back({moveset,Helpers::to_unicode(moveset->name.c_str()),
        Helpers::to_unicode(moveset->origin.c_str())});
    std::sort(rows.begin(),rows.end(),[&](const Row& a,const Row& b) {
        if (specs) for (int i = 0; i < specs->SpecsCount; ++i) {
            const auto& spec = specs->Specs[i];
            int order = spec.ColumnUserID == Game ? Compare(a.game,b.game) : Compare(a.name,b.name);
            if (order) return spec.SortDirection == ImGuiSortDirection_Descending ? order > 0 : order < 0;
        }
        if (auto order = Compare(a.name,b.name)) return order < 0;
        // Equal names in multiple folders keep a deterministic order.
        return a.moveset->filename < b.moveset->filename;
    });
    std::vector<movesetInfo*> result;
    result.reserve(rows.size());
    for (const auto& row : rows) result.push_back(row.moveset);
    return result;
}
inline std::vector<movesetInfo*> TableView(std::span<movesetInfo* const> source) {
    auto* specs = ImGui::TableGetSortSpecs();
    auto view = View(source,specs);
    if (specs) specs->SpecsDirty = false;
    return view;
}

// One category per game. Rows keep the table's sort order inside their category;
// categories follow the Game column direction (ascending unless sorted descending).
struct Group { std::string game; std::vector<movesetInfo*> movesets; };
inline std::vector<Group> GroupedTableView(std::span<movesetInfo* const> source) {
    auto* specs = ImGui::TableGetSortSpecs();
    bool descending = false;
    if (specs) for (int i = 0; i < specs->SpecsCount; ++i)
        if (specs->Specs[i].ColumnUserID == Game) descending = specs->Specs[i].SortDirection == ImGuiSortDirection_Descending;
    const auto view = TableView(source);
    std::vector<Group> groups;
    for (auto* moveset : view) {
        const char* game = moveset->origin.c_str();
        auto it = std::find_if(groups.begin(),groups.end(),[&](const Group& g) { return g.game == game; });
        if (it == groups.end()) { groups.push_back({std::string(game),{}}); it = groups.end() - 1; }
        it->movesets.push_back(moveset);
    }
    std::sort(groups.begin(),groups.end(),[&](const Group& a,const Group& b) {
        // Unreadable files always go last.
        if ((a.game == "INVALID") != (b.game == "INVALID")) return b.game == "INVALID";
        const int order = Compare(Helpers::to_unicode(a.game.c_str()),Helpers::to_unicode(b.game.c_str()));
        return descending ? order > 0 : order < 0;
    });
    return groups;
}

// -- Search box filter, shared by the moveset lists -- //

// Lower-cased wide copy of a UTF-8 string, for case-insensitive search (works for non-Latin names too)
inline std::wstring LowerForSearch(const std::string& utf8) {
    std::wstring text = Helpers::to_unicode(utf8.c_str());
    std::transform(text.begin(),text.end(),text.begin(),[](wchar_t c) { return (wchar_t)std::towlower(c); });
    return text;
}
// True if the moveset's name, game or target character contains the (lower-cased) search text
inline bool MatchesSearch(const movesetInfo& moveset, const std::wstring& needle) {
    for (const char* field : { moveset.name.c_str(), moveset.origin.c_str(), moveset.target_character.c_str() })
        if (LowerForSearch(field).find(needle) != std::wstring::npos) return true;
    return false;
}
// Keeps the group's movesets that match; false when none does (the game is hidden, header included)
inline bool Filter(Group& group, const std::wstring& needle) {
    if (needle.empty()) return true;
    std::erase_if(group.movesets,[&](const movesetInfo* m) { return !MatchesSearch(*m,needle); });
    return !group.movesets.empty();
}

// Category header row: collapsible, spans the whole table width. Returns true when open.
inline bool GroupHeader(const Group& group, const char* label) {
    ImGui::TableNextRow();
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,ImGui::GetColorU32(ImGuiCol_TableHeaderBg));
    ImGui::TableNextColumn();
    ImGui::PushID(group.game.c_str());
    const bool open = ImGui::TreeNodeEx("##group",ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_DefaultOpen
        | ImGuiTreeNodeFlags_NoTreePushOnOpen,"%s",label);
    ImGui::PopID();
    return open;
}

// Subtle hover tint for a moveset row. Begin() right after the row's first TableNextColumn(),
// End() after its last cell. Drawn on RowBg1 so it blends over a custom moveset colour (RowBg0).
struct RowHover {
    float top = 0;
    void Begin() { top = ImGui::GetCursorScreenPos().y - ImGui::GetStyle().CellPadding.y; }
    void End() const {
        const auto& style = ImGui::GetStyle();
        const float bottom = (std::max)(ImGui::GetItemRectMax().y,top + style.CellPadding.y + ImGui::GetFrameHeight()) + style.CellPadding.y;
        const auto window = ImGui::GetWindowPos();
        if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(ImVec2(window.x,top),ImVec2(window.x + ImGui::GetWindowWidth(),bottom),false))
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,ImGui::GetColorU32(ImGuiCol_HeaderHovered,0.35f));
    }
};
}
