#include "ExeSignatures.hpp"
#include "Extractor_t8/RetailGameData.hpp"

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <format>
#include <fstream>
#include <functional>
#include <regex>
#include <set>
#include <stdexcept>

namespace ExeSignatures {
namespace {

template<class T> T At(std::span<const uint8_t> bytes, size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < sizeof(T)) throw std::runtime_error("Executable headers are truncated");
    T value; memcpy(&value, bytes.data() + offset, sizeof(T)); return value;
}

struct Pattern {
    std::vector<uint8_t> bytes;
    std::vector<bool> fixed;
    size_t capture = 0;       // offset of the 4 captured bytes
    bool ripRelative = false;
    bool position = false;    // [f]: the value is the RVA of the capture point itself
    uint32_t trailing = 0;    // instruction bytes after the displacement ([r4+N])
    size_t anchor = 0, anchorLength = 0; // longest run of fixed bytes, searched first
};

Pattern Parse(const std::string& name, const std::string& text) {
    Pattern p; bool captured = false;
    size_t at = 0;
    auto fail = [&](const std::string& why) { throw std::invalid_argument(std::format("Address pattern '{}': {}", name, why)); };
    while (at < text.size()) {
        while (at < text.size() && (text[at] == ' ' || text[at] == '\t')) ++at;
        if (at >= text.size()) break;
        size_t end = text.find_first_of(" \t", at);
        const std::string token = text.substr(at, end == std::string::npos ? std::string::npos : end - at);
        at = end == std::string::npos ? text.size() : end;
        if (token == "[f]") {
            if (captured) fail("more than one capture");
            captured = true; p.capture = p.bytes.size(); p.position = true;
        }
        else if (token.front() == '[') {
            std::smatch m;
            static const std::regex capture(R"(\[(r|v)4(?:\+(\d{1,2}))?\])");
            if (!std::regex_match(token, m, capture)) fail("unknown capture " + token);
            if (captured) fail("more than one capture");
            if (m[1] == "v" && m[2].matched) fail("[v4] takes no +N");
            captured = true; p.capture = p.bytes.size(); p.ripRelative = m[1] == "r";
            p.trailing = m[2].matched ? static_cast<uint32_t>(std::stoul(m[2].str())) : 0;
            for (int i = 0; i < 4; ++i) { p.bytes.push_back(0); p.fixed.push_back(false); }
        }
        else if (token == "??") { p.bytes.push_back(0); p.fixed.push_back(false); }
        else {
            if (token.size() != 2 || !isxdigit(static_cast<unsigned char>(token[0])) || !isxdigit(static_cast<unsigned char>(token[1])))
                fail("bad byte '" + token + "'");
            p.bytes.push_back(static_cast<uint8_t>(std::stoul(token, nullptr, 16))); p.fixed.push_back(true);
        }
    }
    if (!captured) fail("no capture ([r4], [r4+N], [v4] or [f])");
    if (p.position && p.capture >= p.bytes.size()) fail("[f] must be followed by bytes");
    for (size_t i = 0; i < p.bytes.size();) {
        if (!p.fixed[i]) { ++i; continue; }
        size_t j = i; while (j < p.bytes.size() && p.fixed[j]) ++j;
        if (j - i > p.anchorLength) { p.anchor = i; p.anchorLength = j - i; }
        i = j;
    }
    // Too few fixed bytes make the search slow and a unique match meaningless.
    if (p.anchorLength < 2) fail("needs at least 2 consecutive fixed bytes");
    if (std::count(p.fixed.begin(), p.fixed.end(), true) < 6) fail("needs at least 6 fixed bytes");
    return p;
}

bool Matches(const Pattern& p, const uint8_t* at) {
    for (size_t i = 0; i < p.bytes.size(); ++i) if (p.fixed[i] && at[i] != p.bytes[i]) return false;
    return true;
}

}

namespace {
// Section table only; `code` is sized but left empty.
Image ParseHeaders(std::span<const uint8_t> headers) {
    Image image;
    if (At<uint16_t>(headers, 0) != 0x5A4D) throw std::runtime_error("Not an executable (no MZ header)");
    const auto pe = At<uint32_t>(headers, 0x3C);
    if (At<uint32_t>(headers, pe) != 0x00004550) throw std::runtime_error("Not a PE executable");
    const auto count = At<uint16_t>(headers, pe + 6);
    image.peTimestamp = At<uint32_t>(headers, pe + 8);
    const auto optional = At<uint16_t>(headers, pe + 20);
    if (count == 0 || count > 96) throw std::runtime_error("Executable section table is invalid");
    for (size_t i = 0, at = static_cast<size_t>(pe) + 24 + optional; i < count; ++i, at += 40) {
        Section s;
        char name[9]{}; for (int k = 0; k < 8; ++k) name[k] = static_cast<char>(At<uint8_t>(headers, at + k));
        s.name = name;
        s.virtualSize = At<uint32_t>(headers, at + 8); s.virtualAddress = At<uint32_t>(headers, at + 12);
        s.rawSize = At<uint32_t>(headers, at + 16); s.rawOffset = At<uint32_t>(headers, at + 20);
        s.characteristics = At<uint32_t>(headers, at + 36);
        image.sections.push_back(s);
    }
    image.code.resize(image.sections.size());
    return image;
}
}

Image ParseImage(std::span<const uint8_t> file) {
    Image image = ParseHeaders(file);
    for (size_t i = 0; i < image.sections.size(); ++i) {
        const auto& s = image.sections[i];
        if (!s.Executable() || !s.rawSize) continue;
        if (s.rawOffset > file.size() || file.size() - s.rawOffset < s.rawSize) throw std::runtime_error("Executable section " + s.name + " is truncated");
        image.code[i].assign(file.begin() + s.rawOffset, file.begin() + s.rawOffset + s.rawSize);
    }
    return image;
}

Image ReadImage(const std::filesystem::path& exe) {
    std::ifstream in(exe, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open " + exe.string());
    in.seekg(0, std::ios::end); const auto size = static_cast<uint64_t>(in.tellg());
    auto read = [&](uint64_t at, uint64_t length) {
        if (at > size || size - at < length) throw std::runtime_error("Executable is truncated");
        std::vector<uint8_t> bytes(static_cast<size_t>(length));
        in.clear(); in.seekg(static_cast<std::streamoff>(at));
        if (length && !in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(length))) throw std::runtime_error("Cannot read " + exe.string());
        return bytes;
    };
    // Headers (the section table lives in the first page), then only the executable sections: a pattern only matches code.
    Image image = ParseHeaders(read(0, std::min<uint64_t>(size, 0x2000)));
    for (size_t i = 0; i < image.sections.size(); ++i)
        if (image.sections[i].Executable() && image.sections[i].rawSize)
            image.code[i] = read(image.sections[i].rawOffset, image.sections[i].rawSize);
    return image;
}

std::string TargetKey(std::string_view patternKey) {
    if (!patternKey.starts_with("sig_")) return {};
    const auto underscore = patternKey.find_last_of('_');
    if (underscore <= 4 || underscore + 1 >= patternKey.size()) return {};
    for (auto c : patternKey.substr(underscore + 1)) if (!isdigit(static_cast<unsigned char>(c))) return {};
    return std::string(patternKey.substr(4, underscore - 4));
}

ScanReport Scan(const Image& image, const std::map<std::string, std::string>& patterns) {
    ScanReport report; report.peTimestamp = image.peTimestamp;
    std::map<std::string, KeyResult> keys;
    for (const auto& [name, text] : patterns) {
        const auto target = TargetKey(name);
        if (target.empty()) continue;
        const auto pattern = Parse(name, text);
        PatternResult result; result.name = name;
        size_t hits = 0; int64_t value = 0; std::string invalid;
        const std::boyer_moore_horspool_searcher searcher(pattern.bytes.begin() + pattern.anchor,
            pattern.bytes.begin() + pattern.anchor + pattern.anchorLength);
        for (size_t s = 0; s < image.sections.size() && hits < 2; ++s) {
            const auto& code = image.code[s];
            if (code.size() < pattern.bytes.size()) continue;
            for (auto it = code.begin(); hits < 2;) {
                it = std::search(it, code.end(), searcher);
                if (it == code.end()) break;
                const size_t anchorAt = static_cast<size_t>(it - code.begin());
                ++it;
                if (anchorAt < pattern.anchor) continue;
                const size_t start = anchorAt - pattern.anchor;
                if (code.size() - start < pattern.bytes.size() || !Matches(pattern, code.data() + start)) continue;
                ++hits;
                const auto& section = image.sections[s];
                if (pattern.position) {
                    // Only executable sections are searched, so the position is code.
                    value = static_cast<int64_t>(section.virtualAddress) + static_cast<int64_t>(start + pattern.capture);
                    invalid.clear();
                    continue;
                }
                int32_t captured; memcpy(&captured, code.data() + start + pattern.capture, 4);
                if (pattern.ripRelative) {
                    const int64_t end = static_cast<int64_t>(section.virtualAddress) + static_cast<int64_t>(start + pattern.capture) + 4 + pattern.trailing;
                    value = end + captured;
                    // globals (writable data), call targets (code) and vtables (read-only data) are all captured this
                    // way (Tag's shared-animation keys use the last two); only a target outside the image is rejected
                    const bool inImage = std::any_of(image.sections.begin(), image.sections.end(), [&](const Section& d) {
                        return value >= d.virtualAddress && value < static_cast<int64_t>(d.virtualAddress) + (std::max)(d.virtualSize, d.rawSize); });
                    invalid = inImage ? "" : std::format("points to {} outside the executable's sections", Hex(value));
                }
                else {
                    value = captured;
                    invalid = value > 0 && value < 0x100000 ? "" : std::format("value {} is not a plausible offset", Hex(value));
                }
            }
        }
        if (hits == 0) result.status = PatternStatus::NotFound;
        else if (hits > 1) result.status = PatternStatus::Ambiguous;
        else if (!invalid.empty()) { result.status = PatternStatus::Invalid; result.detail = invalid; }
        else { result.status = PatternStatus::Found; result.value = value; }
        auto& key = keys[target]; key.key = target;
        key.patterns.push_back(result);
    }
    for (auto& [name, key] : keys) {
        std::vector<int64_t> values;
        for (const auto& p : key.patterns) if (p.status == PatternStatus::Found) values.push_back(p.value);
        if (values.empty()) key.status = KeyStatus::NotFound;
        else if (std::any_of(values.begin(), values.end(), [&](int64_t v) { return v != values.front(); })) key.status = KeyStatus::Conflict;
        else { key.status = KeyStatus::Found; key.value = values.front(); }
        report.keys.push_back(std::move(key));
    }
    return report;
}

std::string Hex(int64_t value) { return value < 0 ? std::format("-0x{:X}", -value) : std::format("0x{:X}", value); }

std::string RewriteIni(std::string_view text, const std::string& gameKey, const std::map<std::string, int64_t>& updates,
    std::vector<std::string>& missing) {
    std::string out; out.reserve(text.size() + 64);
    std::set<std::string> done;
    size_t at = 0;
    while (at < text.size()) {
        size_t lineEnd = text.find('\n', at);
        const size_t next = lineEnd == std::string_view::npos ? text.size() : lineEnd + 1;
        std::string line(text.substr(at, next - at));
        at = next;
        // Line body without its line ending.
        size_t bodyEnd = line.size();
        while (bodyEnd && (line[bodyEnd - 1] == '\n' || line[bodyEnd - 1] == '\r')) --bodyEnd;
        const size_t keyStart = line.find_first_not_of(" \t");
        const size_t equals = line.find('=');
        const size_t comment = line.find('#');
        if (keyStart == std::string::npos || keyStart >= bodyEnd || line[keyStart] == '#' || equals == std::string::npos ||
            equals >= bodyEnd || (comment != std::string::npos && comment < equals)) { out += line; continue; }
        std::string key = line.substr(keyStart, equals - keyStart);
        key.erase(key.find_last_not_of(" \t") + 1);
        bool value = false;
        for (const char* prefix : { "val:", "v:" }) if (key.starts_with(prefix)) { key = key.substr(strlen(prefix)); value = true; }
        if (!key.starts_with(gameKey + "_")) { out += line; continue; }
        const auto shortKey = key.substr(gameKey.size() + 1);
        const auto update = updates.find(shortKey);
        if (update == updates.end()) { out += line; continue; }
        const size_t valueStart = line.find_first_not_of(" \t", equals + 1);
        if (valueStart == std::string::npos || valueStart >= bodyEnd) { out += line; continue; }
        size_t valueEnd;
        size_t replaceStart = valueStart;
        if (value) valueEnd = line.find_first_of(" \t#\r\n", valueStart);
        else {
            // Relative pointer path: replace only the module offset (first element after '+').
            if (line[valueStart] != '+') { out += line; continue; }
            replaceStart = valueStart + 1;
            valueEnd = line.find_first_of(", \t#\r\n", replaceStart);
        }
        if (valueEnd == std::string::npos) valueEnd = bodyEnd;
        const auto replacement = Hex(update->second);
        const auto oldLength = valueEnd - replaceStart;
        line.replace(replaceStart, oldLength, replacement);
        // Keep a trailing comment in its column when the spacing before it allows it.
        if (value && replacement.size() != oldLength) {
            const size_t after = replaceStart + replacement.size();
            const size_t hash = line.find('#', after);
            if (hash != std::string::npos && line.find_first_not_of(" \t", after) == hash) {
                const auto spaces = hash - after;
                const auto wanted = static_cast<int64_t>(spaces) + static_cast<int64_t>(oldLength) - static_cast<int64_t>(replacement.size());
                line.replace(after, spaces, std::string(static_cast<size_t>(std::max<int64_t>(1, wanted)), ' '));
            }
        }
        done.insert(shortKey);
        out += line;
    }
    for (const auto& [key, v] : updates) if (!done.count(key)) missing.push_back(key);
    return out;
}

namespace {
std::optional<std::wstring> SteamPath() {
    wchar_t buffer[MAX_PATH * 2]; DWORD size = sizeof(buffer);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS)
        return std::nullopt;
    return std::wstring(buffer);
}
// Quoted values on lines like `"path"   "F:\\SteamLibrary"` or `"installdir"   "TEKKEN 8"`.
std::vector<std::string> VdfValues(const std::filesystem::path& file, std::string_view field) {
    std::vector<std::string> values;
    std::ifstream in(file);
    std::string line;
    const std::string quoted = "\"" + std::string(field) + "\"";
    while (std::getline(in, line)) {
        const auto key = line.find(quoted);
        if (key == std::string::npos) continue;
        const auto open = line.find('"', key + quoted.size());
        const auto close = open == std::string::npos ? std::string::npos : line.find('"', open + 1);
        if (close == std::string::npos) continue;
        std::string value;
        for (size_t i = open + 1; i < close; ++i) {
            if (line[i] == '\\' && i + 1 < close) ++i;
            value += line[i];
        }
        values.push_back(value);
    }
    return values;
}
}

namespace {
// Absolute install folder Steam records in the uninstall key `Steam App <appId>` (64-bit registry view).
std::optional<std::filesystem::path> UninstallLocation(const std::wstring& appId) {
    const std::wstring key = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App " + appId;
    wchar_t buffer[MAX_PATH * 2]; DWORD size = sizeof(buffer);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, key.c_str(), L"InstallLocation", RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, buffer, &size) != ERROR_SUCCESS)
        return std::nullopt;
    return std::filesystem::path(buffer);
}

// Executable `exe` (relative to the game folder) of the Steam app `appId`: uninstall key first, then every Steam library.
std::optional<std::filesystem::path> FindSteamExecutable(const std::wstring& appId, const char* defaultDir, const std::filesystem::path& exe) {
    std::error_code error;
    if (auto folder = UninstallLocation(appId)) {
        const auto path = *folder / exe;
        if (std::filesystem::is_regular_file(path, error)) return path;
    }
    const auto steam = SteamPath();
    if (!steam) return std::nullopt;
    std::vector<std::filesystem::path> libraries{ std::filesystem::path(*steam) };
    for (const auto& path : VdfValues(std::filesystem::path(*steam) / "steamapps" / "libraryfolders.vdf", "path"))
        libraries.push_back(std::filesystem::u8path(path));
    for (const auto& library : libraries) {
        const auto manifest = library / "steamapps" / (L"appmanifest_" + appId + L".acf");
        if (!std::filesystem::exists(manifest, error)) continue;
        const auto dirs = VdfValues(manifest, "installdir");
        const auto path = library / "steamapps" / "common" / std::filesystem::u8path(dirs.empty() ? defaultDir : dirs.front()) / exe;
        if (std::filesystem::is_regular_file(path, error)) return path;
    }
    return std::nullopt;
}
}

std::optional<std::filesystem::path> FindT8Executable() {
    if (auto running = RetailT8GameData::RunningGameExecutable()) return running;
    return FindInstalledT8Executable();
}

std::optional<std::filesystem::path> FindInstalledT8Executable() {
    return FindSteamExecutable(L"1778820", "TEKKEN 8", std::filesystem::path("Polaris") / "Binaries" / "Win64" / "Polaris-Win64-Shipping.exe");
}

std::optional<std::filesystem::path> FindInstalledT7Executable() {
    return FindSteamExecutable(L"389730", "TEKKEN 7", std::filesystem::path("TekkenGame") / "Binaries" / "Win64" / "TekkenGame-Win64-Shipping.exe");
}

}
