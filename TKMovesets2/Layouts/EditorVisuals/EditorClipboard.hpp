#pragma once
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Clipboard transport is independent of game layouts, editor mutations and ImGui.
namespace EditorClipboard {
inline constexpr size_t MaxBytes = 4 * 1024 * 1024;
inline constexpr size_t MaxFields = 65536;
using Rows = std::vector<std::vector<std::string>>;
inline std::string_view Text(const char* text) {
    if (!text) throw std::invalid_argument("The clipboard contains no text.");
    size_t size = 0;
    while (size <= MaxBytes && text[size]) ++size;
    if (size > MaxBytes) throw std::length_error("Clipboard text exceeds 4 MiB.");
    return {text, size};
}
inline Rows Decode(std::string_view text, std::string_view type, std::span<const size_t> widths, size_t maxRows) {
    if (text.size() > MaxBytes || text.find('\0') != text.npos) throw std::invalid_argument("Invalid clipboard text size or embedded NUL.");
    if (widths.empty() || !maxRows || widths.size() > MaxFields) throw std::invalid_argument("This form has no pasteable fields.");
    auto line = [&]() {
        auto end = text.find('\n');
        auto value = text.substr(0, end);
        text = end == text.npos ? std::string_view{} : text.substr(end + 1);
        if (!value.empty() && value.back() == '\r') value.remove_suffix(1);
        return value;
    };
    if (text.find('\n') == text.npos || line() != type) throw std::invalid_argument("Clipboard structure type does not match this form.");
    Rows result;
    size_t fields = 0;
    while (!text.empty()) {
        if (fields >= MaxFields || fields / widths.size() >= maxRows) throw std::length_error("Too many clipboard rows.");
        auto value = line();
        auto column = fields % widths.size();
        if (!widths[column] || value.size() >= widths[column]) throw std::length_error("A clipboard value exceeds its field capacity.");
        if (!column) result.emplace_back();
        result.back().emplace_back(value);
        ++fields;
    }
    if (!fields || fields % widths.size()) throw std::invalid_argument("Clipboard data ends inside a structure.");
    return result;
}
}
