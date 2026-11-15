#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// T7 <-> retail T8 conversion rules. Edited in Rules/t7_t8_conversion_rules.ini; the build merges them into a marked
// section of Resources/game_addresses.ini, so the rules are embedded, downloaded and updated with the game addresses.
// The section of game_addresses.ini next to the program replaces the built-in rules when it is complete, valid, of a
// format this build understands and not older; anything else keeps the built-in rules. Layouts, field encryption and
// animation code are not rules: they stay in the converters. See docs/conversion-rules.md.
namespace ConversionRules {
inline constexpr uint32_t SupportedFormat = 1;
inline constexpr const char* SectionBegin = "# BEGIN T7 T8 CONVERSION RULES";
inline constexpr const char* SectionEnd = "# END T7 T8 CONVERSION RULES";
enum Direction { ToT8 = 0, ToT7 = 1 };
// Per-moveset import settings the rules read through setting(NAME) (ImportProfile::ConversionSettings).
using Settings = std::map<std::string, uint32_t>;
// The names a rule may read, with the values a moveset without an import profile uses.
const Settings& DefaultSettings();

// Map lines "T7 <-> T8", "T7 -> T8", "T7 <- T8"; each side a value, a range a..b or an open range a.. ("none" for
// a value that has no counterpart in that direction).
struct IdMap {
    struct Line { std::array<uint32_t, 2> first{}, last{}; std::array<bool, 2> forward{}; std::array<bool, 2> none{}; uint32_t line = 0; };
    std::vector<Line> lines;
    enum class Result { NoRule, None, Mapped };
    // `x` is a T7 value for ToT8 and a T8 value for ToT7.
    Result Find(Direction direction, uint32_t x, uint32_t& out) const;
    std::optional<uint32_t> Map(Direction direction, uint32_t x) const {
        uint32_t out; return Find(direction, x, out) == Result::Mapped ? std::optional<uint32_t>(out) : std::nullopt;
    }
    bool Has(Direction direction, uint32_t x) const { uint32_t out; return Find(direction, x, out) != Result::NoRule; }
};

// Parameter transform: "t7_to_t8_prop:SOURCE -> TARGET  p0 = EXPR ; p2 = EXPR [; miss = drop]" (T7 value v -> retail
// parameters p0..p4) or "t8_to_t7_prop:SOURCE -> TARGET  v = EXPR [; id = EXPR] [; miss = drop]" (retail parameters ->
// T7 value; "id = EXPR" picks the T7 id from the parameters instead of TARGET, and a computed 0 / 881 / 1100 drops the row).
struct Transform {
    struct Node { enum Op { Const, V, P, Lookup, Or, Xor, And, Shl, Shr, Add, Sub, Eq, Ne, Setting, Lt, Le, Gt, Ge } op; uint32_t value = 0; int a = -1, b = -1; std::string table; };
    Direction direction = ToT8;
    uint32_t source = 0, target = 0, line = 0;
    bool missDrop = false;
    bool condition = false; // "t7_to_t8_req:ID = EXPR" / "t8_to_t7_req:ID = EXPR": when the requirement holds
    // "t8_to_t7_fix:KIND:ID  [drop = EXPR ;] [v = EXPR ;] [miss = drop]": a fix on a T7 record the T8 -> T7 converter
    // writes (FixKinds). `source` is the T7 id or field offset; `drop` the root of the drop condition (-1: none).
    std::string fix;
    int drop = -1;
    // "t8_to_t7_guard:ID  [when = EXPR ;] id = EXPR ; v = EXPR": a T7 requirement list with no condition (only rows
    // 0 and properties 0x8000 and up) holding row ID with value v where `when` holds gets the row {id, v} in front.
    bool guard = false;
    int when = -1;
    std::vector<Node> nodes;
    std::vector<std::pair<int, int>> sets; // destination (-1 = v, 0..4 = p0..p4), root node
};

// t8_to_t7_fix kinds: what the T7 record is, what ID names and what v holds. A fix runs on the converted T7 value
// (after every map and transform), so it reaches a value whatever retail id or table it came from.
//   prop ID        extra/start/end property row with T7 id ID; v = T7 value, p0..p4 = retail parameters. drop: row left out.
//   req ID         requirement row with T7 id ID; v = T7 parameter, p0..p4 = retail parameters. drop: never true.
//   voice CATEGORY voiceclip of T7 category CATEGORY; v = clip id (24 bits). drop: clip left out.
//   cancel CMD     cancel or group cancel with T7 command CMD; v = target move id. drop: never true.
//   move OFFSET    u32 field at OFFSET of every T7 move (0xB0 bytes, not a pointer); v = field. No drop.
//   projectile OFFSET  u32 field at OFFSET of every T7 projectile (0xA8 bytes, not a pointer); v = field. No drop.
inline constexpr const char* FixKinds[] = {"prop", "req", "voice", "cancel", "move", "projectile"};
struct FixResult { bool drop = false, changed = false; uint32_t v = 0; };

struct Rules {
    uint32_t version = 0, format = 0;
    std::map<std::string, uint32_t> values;
    std::map<std::string, std::vector<std::pair<uint32_t, uint32_t>>> lists; // inclusive spans
    std::map<std::string, IdMap> maps;
    std::map<std::string, std::map<uint32_t, uint32_t>> tables;
    std::vector<std::pair<uint32_t, uint32_t>> t8MoveDefaults; // record offset, value
    std::vector<Transform> transforms;

    uint32_t Value(const std::string& name) const;
    bool InList(const std::string& name, uint32_t value) const;
    const IdMap& Map(const std::string& name) const;
    // A one-line "T7 <-> T8" map: {T7 value, T8 value}.
    std::array<uint32_t, 2> Pair(const std::string& name) const;
    enum class Applied { None, Drop, Mapped };
    // T7 property `id` with value `v` -> retail id and parameters. `settings`: what setting(NAME) reads.
    Applied ToT8(uint32_t id, uint32_t v, uint32_t& target, std::array<uint32_t, 5>& p, const Settings& settings = DefaultSettings()) const;
    // Retail property `id` with parameters `p` -> T7 id and value.
    Applied ToT7(uint32_t id, const std::array<uint32_t, 5>& p, uint32_t& target, uint32_t& v, const Settings& settings = DefaultSettings()) const;
    // Requirement conditions: whether requirement `id` (T7 value v, or retail parameters p) holds in normal play.
    // nullopt when no condition is given for it. The converters write an always-true or a never-true row.
    std::optional<bool> RequirementHolds(Direction direction, uint32_t id, uint32_t v, const std::array<uint32_t, 5>& p,
        const Settings& settings = DefaultSettings()) const;
    // t8_to_t7_fix: the fix for T7 record `kind` with id/offset `id` and value `v` (FixKinds). Without a fix (or when
    // its drop condition is false and it sets no v) the result is {false, false, v}.
    FixResult Fix(std::string_view kind, uint32_t id, uint32_t v, const std::array<uint32_t, 5>& p = {},
        const Settings& settings = DefaultSettings()) const;
    // t8_to_t7_guard: the condition row {id, value} to put in front of a condition-less requirement list holding T7
    // row {id, v}; nullopt without a guard rule for `id`, when `when` is false, or when the computed id is no
    // condition (0, 881 or 0x8000 and up).
    std::optional<std::pair<uint32_t, uint32_t>> Guard(uint32_t id, uint32_t v, const Settings& settings = DefaultSettings()) const;
    // The ids/offsets that have a fix of `kind` (for the field kinds, which the converter walks).
    std::vector<uint32_t> FixIds(std::string_view kind) const;
};

// Throws std::runtime_error naming the line for any malformed, incomplete or unsafe rule.
Rules Parse(std::string_view text);
// The rules section of a game_addresses.ini (the whole text when it has no section markers: a bare rules file).
std::string Section(std::string_view addresses);
// The rules embedded in this build (the section of the embedded game_addresses.ini).
const Rules& Embedded();
std::string_view EmbeddedText();
// The rules the converters use: the downloaded file when it qualifies, else the embedded ones. Read once per run.
const Rules& Current();
struct Status { std::string source, error; uint32_t version = 0, embeddedVersion = 0; };
Status CurrentStatus();
}
