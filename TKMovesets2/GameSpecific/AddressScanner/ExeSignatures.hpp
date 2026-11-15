#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Finds game addresses by searching the game's executable FILE on disk for byte patterns stored in game_addresses.ini.
// Nothing here touches a running game: the exe is read like any other file.
//
// Pattern syntax (str:<game>_sig_<key>_<n> = ...):
//   "48", "8B"   a fixed byte (hex)
//   "??"         any byte
//   "[r4]"       capture: 4-byte RIP-relative displacement. The result is the RVA it points to
//                (end of the instruction + displacement), and must land in a writable data section.
//   "[r4+N]"     same, for an instruction with N more bytes after the displacement (e.g. an immediate).
//   "[v4]"       capture: 4-byte value taken as is (a struct offset), must be in (0, 0x100000).
//   "[f]"        capture, zero-length: the RVA of the next pattern byte (a function start, in executable code).
// Every pattern holds exactly one capture. A key may have several patterns (_1, _2, ...): each one that matches
// exactly once in the executable's code yields a value, and every such value must agree.
namespace ExeSignatures {

enum class PatternStatus { Found, NotFound, Ambiguous, Invalid };
enum class KeyStatus { Found, NotFound, Conflict };

struct PatternResult {
    std::string name;      // ini short key, e.g. sig_registry_rva_2
    PatternStatus status = PatternStatus::NotFound;
    int64_t value = 0;     // valid when Found
    std::string detail;    // why it was rejected (Invalid)
};

struct KeyResult {
    std::string key;       // ini short key the value belongs to, e.g. registry_rva
    KeyStatus status = KeyStatus::NotFound;
    int64_t value = 0;     // valid when Found
    std::vector<PatternResult> patterns;
};

struct ScanReport {
    uint32_t peTimestamp = 0;
    std::vector<KeyResult> keys;
};

struct Section {
    std::string name;
    uint32_t virtualAddress = 0, virtualSize = 0, rawOffset = 0, rawSize = 0, characteristics = 0;
    bool Executable() const { return characteristics & 0x20000000; }
    bool WritableData() const { return (characteristics & 0x80000000) && !Executable(); }
};

struct Image {
    uint32_t peTimestamp = 0;
    std::vector<Section> sections;
    // Raw bytes of each executable section, indexed like `sections` (empty for the others).
    std::vector<std::vector<uint8_t>> code;
};

// Parses a PE image held in memory (tests) or read from disk (only headers + executable sections are read).
Image ParseImage(std::span<const uint8_t> file);
Image ReadImage(const std::filesystem::path& exe);

// patterns: ini short key ("sig_<key>_<n>") -> pattern text. Throws std::invalid_argument on a malformed pattern.
ScanReport Scan(const Image& image, const std::map<std::string, std::string>& patterns);

// ini short key of the value a pattern resolves ("sig_registry_rva_2" -> "registry_rva"); empty if not a pattern key.
std::string TargetKey(std::string_view patternKey);

// Rewrites values in game_addresses.ini text, keeping every other byte (comments, spacing, line endings) as is.
// For `val:<game>_<key>` the value is replaced; for a relative pointer path `<game>_<key> = +A, B, ...` only A is.
// Keys absent from the text are returned in `missing` and never inserted.
std::string RewriteIni(std::string_view text, const std::string& gameKey, const std::map<std::string, int64_t>& updates,
    std::vector<std::string>& missing);

std::string Hex(int64_t value);

// Retail Tekken 8 executable: the running game's if it is open, else the Steam install (app 1778820).
std::optional<std::filesystem::path> FindT8Executable();

// Absolute exe path of the Steam install, found without the game running (uninstall registry key, then Steam libraries).
std::optional<std::filesystem::path> FindInstalledT8Executable();
std::optional<std::filesystem::path> FindInstalledT7Executable();

}
