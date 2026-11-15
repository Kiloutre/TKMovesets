#include "ConversionRules.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include "constants.h"

extern "C" const char game_addresses_ini[];
extern "C" const unsigned game_addresses_ini_size;

namespace ConversionRules {
namespace {
constexpr size_t TextLimit = 4u << 20, LineLimit = 100000;
// Keys over every table: of a whole file, not per item (a 4 MiB file of 65536-key ranges would ask for billions of
// map nodes). The shipped rules hold 19.
constexpr uint64_t TableEntryLimit = 1u << 20;
// Expressions are parsed and evaluated recursively: bound the parentheses/lookups nesting and the tree depth so a
// malformed downloaded file is refused instead of overflowing the stack.
constexpr int NestingLimit = 32, DepthLimit = 64;
constexpr uint32_t T8MoveSize = 0x448;
// t8_to_t7_fix field kinds: record size and the 8-byte reference fields a fix may not touch (rows of other tables).
constexpr uint32_t T7MoveSize = 0xB0, T7ProjectileSize = 0xA8;
constexpr uint32_t T7MovePointers[] = {0x00, 0x08, 0x10, 0x20, 0x28, 0x38, 0x48, 0x60, 0x78, 0x80, 0x88, 0x90};
constexpr uint32_t T7ProjectilePointers[] = {0x60, 0x68};
// Transform::sets destination of "id = EXPR" in a t8_to_t7_prop (-1 = v, 0..4 = p0..p4, -2 = a condition).
constexpr int IdDestination = -3;
const char* const RequiredValues[] = {
    "t7_t8_rules_version", "t7_t8_rules_format", "t7_to_t8_req_highest_case", "t8_to_t7_req_never_true", "t7_to_t8_req_never_true",
    "t7_to_t8_req_never_true_param", "t7_t8_req_unknown_zero_params_true", "t8_to_t7_placeholder_character", "t7_t8_walk_cancel_t7_frame",
    "t7_t8_walk_cancel_t8_frame", "t7_t8_prop_alias_block_size"};
const char* const RequiredMaps[] = {"t7_t8_cmd", "t7_t8_walk", "t7_t8_alias_slot", "t7_t8_character", "t7_t8_vfx", "t7_t8_prop_alias_block", "t7_to_t8_voice_fixed_id",
    "t7_t8_projectile_prop", "t7_t8_throw_prop"};
const char* const OptionalMaps[] = {"t7_t8_req", "t7_t8_prop"};
const char* const RequiredLists[] = {"t7_to_t8_req_known_false", "t7_to_t8_req_default_false", "t7_t8_drop_t8_props", "t8_to_t7_voice_drop"};

struct Error : std::runtime_error {
    Error(uint32_t line, const std::string& what) : std::runtime_error("conversion rules line " + std::to_string(line) + ": " + what) {}
};
std::string Trim(std::string_view s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return std::string(s.substr(a, b - a));
}
std::vector<std::string> Split(const std::string& s, char separator) {
    std::vector<std::string> parts; std::string part; std::istringstream in(s);
    while (std::getline(in, part, separator)) parts.push_back(Trim(part));
    return parts;
}
bool Number(const std::string& text, uint32_t& out) {
    const bool hex = text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    const auto digits = hex ? text.substr(2) : text;
    if (digits.empty() || digits.size() > 10) return false;
    uint64_t value = 0;
    for (char c : digits) {
        int digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (hex && c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (hex && c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else return false;
        value = value * (hex ? 16 : 10) + static_cast<uint64_t>(digit);
    }
    if (value > UINT32_MAX) return false;
    out = static_cast<uint32_t>(value); return true;
}
uint32_t NumberOrThrow(uint32_t line, const std::string& text) {
    uint32_t value;
    if (!Number(text, value)) throw Error(line, "not a number: '" + text + "'");
    return value;
}
struct Span { uint32_t first = 0, last = 0; bool none = false, open = false; };
Span ParseSpan(uint32_t line, const std::string& text, bool allowOpen, bool allowNone) {
    Span span;
    if (text == "none") { if (!allowNone) throw Error(line, "'none' is not allowed here"); span.none = true; return span; }
    const auto dots = text.find("..");
    if (dots == std::string::npos) { span.first = span.last = NumberOrThrow(line, text); return span; }
    span.first = NumberOrThrow(line, Trim(text.substr(0, dots)));
    const auto rest = Trim(text.substr(dots + 2));
    if (rest.empty()) {
        if (!allowOpen) throw Error(line, "open range not allowed here");
        span.last = UINT32_MAX; span.open = true; return span;
    }
    span.last = NumberOrThrow(line, rest);
    if (span.last < span.first) throw Error(line, "range ends before it starts");
    return span;
}

// ---- Expressions ------------------------------------------------------
struct Parser {
    Transform& t; uint32_t line; std::string text; size_t at = 0;
    int nesting = 0;
    std::vector<int> depths; // tree depth of each node this parser added (indexed like t.nodes)
    struct Nest {
        Parser& p;
        explicit Nest(Parser& parser) : p(parser) { if (++p.nesting > NestingLimit) throw Error(p.line, "expression nested too deeply"); }
        ~Nest() { --p.nesting; }
    };
    int Depth(int index) const { return index >= 0 && static_cast<size_t>(index) < depths.size() ? depths[static_cast<size_t>(index)] : 0; }
    void Space() { while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at]))) ++at; }
    bool Eat(std::string_view token) { Space(); if (text.compare(at, token.size(), token) == 0) { at += token.size(); return true; } return false; }
    int Add(Transform::Node node) {
        const int depth = 1 + std::max(Depth(node.a), Depth(node.b));
        if (depth > DepthLimit) throw Error(line, "expression too long or nested too deeply");
        t.nodes.push_back(std::move(node));
        depths.resize(t.nodes.size()); depths.back() = depth;
        return static_cast<int>(t.nodes.size() - 1);
    }
    int Binary(int (Parser::*next)(), std::initializer_list<std::pair<std::string_view, Transform::Node::Op>> ops) {
        int left = (this->*next)();
        for (;;) {
            bool matched = false;
            for (auto [token, op] : ops) {
                Space();
                // "<" alone is not an operator: only match complete tokens ("<<" before "<").
                if (text.compare(at, token.size(), token) != 0) continue;
                at += token.size(); matched = true;
                const int right = (this->*next)();
                Transform::Node node{op}; node.a = left; node.b = right; left = Add(node);
                break;
            }
            if (!matched) return left;
        }
    }
    // Comparisons bind loosest and do not chain: "p0 == 4", "v & 0xff != 0", "(v >> 8) > 31". Two-character tokens
    // first; "<" and ">" alone only when not the start of "<<" / ">>" (Or() has already taken every shift).
    int Compare() {
        const int left = Or();
        Space();
        for (auto [token, op] : {std::pair<std::string_view, Transform::Node::Op>{"==", Transform::Node::Eq}, {"!=", Transform::Node::Ne},
                 {"<=", Transform::Node::Le}, {">=", Transform::Node::Ge}, {"<", Transform::Node::Lt}, {">", Transform::Node::Gt}}) {
            if (text.compare(at, token.size(), token) != 0) continue;
            if (token.size() == 1 && at + 1 < text.size() && text[at + 1] == token[0]) continue;
            at += token.size();
            Transform::Node node{op}; node.a = left; node.b = Or(); return Add(node);
        }
        return left;
    }
    int Or() { return Binary(&Parser::Xor, {{"|", Transform::Node::Or}}); }
    int Xor() { return Binary(&Parser::And, {{"^", Transform::Node::Xor}}); }
    int And() { return Binary(&Parser::Shift, {{"&", Transform::Node::And}}); }
    int Shift() { return Binary(&Parser::Sum, {{"<<", Transform::Node::Shl}, {">>", Transform::Node::Shr}}); }
    int Sum() { return Binary(&Parser::Unary, {{"+", Transform::Node::Add}, {"-", Transform::Node::Sub}}); }
    int Unary() {
        Space();
        if (Eat("(")) { Nest nest(*this); const int inner = Compare(); if (!Eat(")")) throw Error(line, "missing ')'"); return inner; }
        if (Eat("setting")) {
            if (!Eat("(")) throw Error(line, "setting needs '('");
            Space(); const auto start = at;
            while (at < text.size() && (std::isalnum(static_cast<unsigned char>(text[at])) || text[at] == '_')) ++at;
            Transform::Node node{Transform::Node::Setting}; node.table = text.substr(start, at - start);
            if (node.table.empty() || !Eat(")")) throw Error(line, "setting(NAME) expected");
            return Add(node);
        }
        if (Eat("lookup")) {
            if (!Eat("(")) throw Error(line, "lookup needs '('");
            Space(); const auto start = at;
            while (at < text.size() && (std::isalnum(static_cast<unsigned char>(text[at])) || text[at] == '_')) ++at;
            Transform::Node node{Transform::Node::Lookup}; node.table = text.substr(start, at - start);
            if (node.table.empty() || !Eat(",")) throw Error(line, "lookup(TABLE, EXPR) expected");
            Nest nest(*this);
            node.a = Compare();
            if (!Eat(")")) throw Error(line, "missing ')' after lookup");
            return Add(node);
        }
        if (Eat("v")) return Add({Transform::Node::V});
        if (at + 1 < text.size() && text[at] == 'p' && text[at + 1] >= '0' && text[at + 1] <= '4') {
            Transform::Node node{Transform::Node::P}; node.value = static_cast<uint32_t>(text[at + 1] - '0'); at += 2; return Add(node);
        }
        const auto start = at;
        while (at < text.size() && std::isalnum(static_cast<unsigned char>(text[at]))) ++at;
        Transform::Node node{Transform::Node::Const}; node.value = NumberOrThrow(line, text.substr(start, at - start));
        return Add(node);
    }
};
struct Miss {};
uint32_t Evaluate(const Rules& rules, const Transform& t, int index, uint32_t v, const std::array<uint32_t, 5>& p,
    const Settings& settings = DefaultSettings()) {
    const auto& n = t.nodes[static_cast<size_t>(index)];
    auto a = [&] { return Evaluate(rules, t, n.a, v, p, settings); };
    auto b = [&] { return Evaluate(rules, t, n.b, v, p, settings); };
    switch (n.op) {
    case Transform::Node::Const: return n.value;
    case Transform::Node::V: return v;
    case Transform::Node::P: return p[n.value];
    case Transform::Node::Setting: {
        const auto found = settings.find(n.table);
        return found != settings.end() ? found->second : DefaultSettings().at(n.table);
    }
    case Transform::Node::Lookup: {
        const auto& table = rules.tables.at(n.table);
        const auto found = table.find(a());
        if (found == table.end()) throw Miss{};
        return found->second;
    }
    case Transform::Node::Or: return a() | b();
    case Transform::Node::Eq: return a() == b() ? 1u : 0u;
    case Transform::Node::Ne: return a() != b() ? 1u : 0u;
    case Transform::Node::Lt: return a() < b() ? 1u : 0u;
    case Transform::Node::Le: return a() <= b() ? 1u : 0u;
    case Transform::Node::Gt: return a() > b() ? 1u : 0u;
    case Transform::Node::Ge: return a() >= b() ? 1u : 0u;
    case Transform::Node::Xor: return a() ^ b();
    case Transform::Node::And: return a() & b();
    case Transform::Node::Shl: { const auto s = b(); return s >= 32 ? 0 : a() << s; }
    case Transform::Node::Shr: { const auto s = b(); return s >= 32 ? 0 : a() >> s; }
    case Transform::Node::Add: return a() + b();
    case Transform::Node::Sub: return a() - b();
    }
    return 0;
}
void CheckInputs(const Transform& t) {
    if (t.guard) {
        for (const auto& n : t.nodes) if (n.op == Transform::Node::P) throw Error(t.line, "t8_to_t7_guard reads v, not p0..p4");
        return;
    }
    if (!t.fix.empty()) {
        // A fix reads the T7 value v; retail parameters only where the record came from a retail parameter list.
        const bool params = t.fix == "prop" || t.fix == "req";
        for (const auto& n : t.nodes)
            if (n.op == Transform::Node::P && !params) throw Error(t.line, "t8_to_t7_fix:" + t.fix + " reads v, not p0..p4");
        return;
    }
    for (const auto& n : t.nodes) {
        if (n.op == Transform::Node::V && t.direction == ToT7) throw Error(t.line, "t8_to_t7_prop reads parameters p0..p4, not v");
        if (n.op == Transform::Node::P && t.direction == ToT8) throw Error(t.line, "t7_to_t8_prop reads the T7 value v, not parameters");
    }
}

void ParseMapLine(Rules& rules, uint32_t line, const std::string& name, const std::string& body) {
    IdMap::Line out; out.line = line;
    std::string left, right;
    if (auto at = body.find("<->"); at != std::string::npos) { left = body.substr(0, at); right = body.substr(at + 3); out.forward = {true, true}; }
    else if (auto at2 = body.find("->"); at2 != std::string::npos) { left = body.substr(0, at2); right = body.substr(at2 + 2); out.forward = {true, false}; }
    else if (auto at3 = body.find("<-"); at3 != std::string::npos) { left = body.substr(0, at3); right = body.substr(at3 + 2); out.forward = {false, true}; }
    else throw Error(line, "map line needs '<->', '->' or '<-'");
    const auto a = ParseSpan(line, Trim(left), true, out.forward[ToT7] && !out.forward[ToT8]);
    const auto b = ParseSpan(line, Trim(right), true, out.forward[ToT8] && !out.forward[ToT7]);
    if (a.none && b.none) throw Error(line, "both sides are 'none'");
    if (!a.none && !b.none) {
        if (a.open != b.open) throw Error(line, "an open range must map to an open range");
        if (!a.open && a.last - a.first != b.last - b.first) throw Error(line, "ranges of different lengths");
    }
    out.first = {a.first, b.first}; out.last = {a.none ? a.first : a.last, b.none ? b.first : b.last};
    out.none = {b.none, a.none}; // none[ToT8]: the T7 side maps to nothing in T8
    if (a.none) { out.first[0] = out.last[0] = 0; }
    if (b.none) { out.first[1] = out.last[1] = 0; }
    rules.maps[name].lines.push_back(out);
}

void Validate(Rules& rules) {
    for (const char* name : RequiredValues) if (!rules.values.count(name)) throw Error(0, std::string("missing val:") + name);
    for (const char* name : RequiredMaps) if (!rules.maps.count(name)) throw Error(0, std::string("missing map:") + name);
    for (const char* name : OptionalMaps) rules.maps[name];
    for (const char* name : RequiredLists) if (!rules.lists.count(name)) throw Error(0, std::string("missing list:") + name);
    if (rules.t8MoveDefaults.empty()) throw Error(0, "missing field:t7_to_t8_move");
    // A name this build does not read is a typo or a rule for a newer build (which must raise conv_rules_format).
    for (const auto& [name, value] : rules.values)
        if (std::none_of(std::begin(RequiredValues), std::end(RequiredValues), [&](const char* v) { return name == v; }))
            throw Error(0, "unknown val:" + name);
    for (const auto& [name, list] : rules.lists)
        if (std::none_of(std::begin(RequiredLists), std::end(RequiredLists), [&](const char* l) { return name == l; }))
            throw Error(0, "unknown list:" + name);
    rules.version = rules.values.at("t7_t8_rules_version");
    rules.format = rules.values.at("t7_t8_rules_format");
    if (!rules.version) throw Error(0, "conv_rules_version must be at least 1");
    if (rules.format != SupportedFormat) throw Error(0, "conv_rules_format " + std::to_string(rules.format) + " is not supported by this build");
    for (const auto& [name, map] : rules.maps) {
        const bool known = std::any_of(std::begin(RequiredMaps), std::end(RequiredMaps), [&](const char* m) { return name == m; }) ||
                           std::any_of(std::begin(OptionalMaps), std::end(OptionalMaps), [&](const char* m) { return name == m; });
        if (!known) throw Error(map.lines.empty() ? 0 : map.lines.front().line, "unknown map:" + name);
        // No two lines may claim one source value in the same direction.
        for (int d = 0; d < 2; ++d) {
            const int side = d == ToT8 ? 0 : 1;
            std::vector<const IdMap::Line*> lines;
            for (const auto& l : map.lines) if (l.forward[d]) lines.push_back(&l);
            std::sort(lines.begin(), lines.end(), [&](auto* x, auto* y) { return x->first[side] < y->first[side]; });
            for (size_t i = 1; i < lines.size(); ++i)
                if (lines[i]->first[side] <= lines[i - 1]->last[side])
                    throw Error(lines[i]->line, "map:" + name + " gives a value two rules in one direction");
        }
    }
    auto check = [](bool ok, uint32_t line, const std::string& what) { if (!ok) throw Error(line, what); };
    auto bounded = [&](const char* name, uint32_t t7Max, uint32_t t8Max) {
        for (const auto& l : rules.maps.at(name).lines) {
            check(l.last[0] <= t7Max && l.last[1] <= t8Max, l.line, std::string("map:") + name + " value out of range");
        }
    };
    // List terminators keep their meaning: only 881 <-> 1100 may touch them.
    for (const auto& l : rules.maps.at("t7_t8_req").lines)
        for (int d = 0; d < 2; ++d) {
            if (!l.forward[d] || l.none[d]) continue;
            const int src = d == ToT8 ? 0 : 1, tgt = 1 - src;
            const uint32_t terminator = tgt == 0 ? 881 : 1100;
            if (l.first[tgt] <= terminator && terminator <= l.last[tgt])
                check(l.first[src] == l.last[src] && l.first[src] == (src == 0 ? 881u : 1100u), l.line, "only 881 <-> 1100 may map to a list terminator");
        }
    check(rules.maps.at("t7_t8_req").Map(ToT8, 881) == 1100u && rules.maps.at("t7_t8_req").Map(ToT7, 1100) == 881u, 0, "map:t7_t8_req must map 881 <-> 1100");
    // List ends of the property tables: 0 (extra properties) on both sides, T7 881 / retail 1100 (start and end lists;
    // the property transforms also run on requirement rows). Side 0 is T7, side 1 retail; a side that is 'none' holds
    // no value. Neither a source nor a target may be a list end: a target would end the list early, a source is never
    // read (the converters keep a list end as one).
    auto endsList = [](int side, uint32_t first, uint32_t last) {
        for (uint32_t end : {0u, side == 0 ? 881u : 1100u}) if (first <= end && end <= last) return true;
        return false;
    };
    for (const auto& l : rules.maps.at("t7_t8_prop").lines)
        for (int side = 0; side < 2; ++side)
            check(l.none[1 - side] || !endsList(side, l.first[side], l.last[side]), l.line,
                side == 0 ? "map:t7_t8_prop covers a T7 property list end (0 or 881)" : "map:t7_t8_prop covers a retail property list end (0 or 1100)");
    for (const auto& l : rules.maps.at("t7_t8_cmd").lines)
        for (int side = 0; side < 2; ++side) {
            const bool none = l.none[1 - side];
            check(none || l.first[side] >= 0x8000, l.line, "map:t7_t8_cmd covers special commands (0x8000 and up) only");
            check(none || l.first[side] > 0x8000 || (l.first[0] == 0x8000 && l.first[1] == 0x8000), l.line, "0x8000 ends a cancel list and maps only to itself");
        }
    check(rules.maps.at("t7_t8_cmd").Map(ToT8, 0x8000) == 0x8000u && rules.maps.at("t7_t8_cmd").Map(ToT7, 0x8000) == 0x8000u, 0, "map:t7_t8_cmd must map 0x8000 <-> 0x8000");
    bounded("t7_t8_walk", 0x7fff, 0x7fff);
    bounded("t7_t8_alias_slot", 55, 59);
    for (uint32_t slot = 0; slot < 56; ++slot)
        check(rules.maps.at("t7_t8_alias_slot").Map(ToT8, slot).has_value(), 0, "map:t7_t8_alias_slot must give every T7 slot 0..55 a T8 slot");
    bounded("t7_t8_character", 0xff, 0xff);
    bounded("t7_t8_vfx", UINT32_MAX - 1, UINT32_MAX - 1);
    bounded("t7_to_t8_voice_fixed_id", 0xff, 0xffffff);
    bounded("t7_t8_prop_alias_block", 0xffff, 0xffff);
    const auto size = rules.values.at("t7_t8_prop_alias_block_size");
    check(size >= 1 && size <= 64, 0, "prop_alias_block_size must be 1..64");
    for (const char* frame : {"t7_t8_walk_cancel_t7_frame", "t7_t8_walk_cancel_t8_frame"})
        check(rules.values.at(frame) >= 1 && rules.values.at(frame) <= 0x7fff, 0, std::string(frame) + " must be 1..32767");
    const auto placeholder = rules.values.at("t8_to_t7_placeholder_character");
    check(placeholder <= 0xff && !rules.maps.at("t7_t8_character").Has(ToT8, placeholder), 0, "t7_placeholder_character must be a free T7 character id");
    for (const char* pair : {"t7_t8_projectile_prop", "t7_t8_throw_prop"}) {
        const auto& lines = rules.maps.at(pair).lines;
        check(lines.size() == 1 && lines[0].forward[ToT8] && lines[0].forward[ToT7] && lines[0].first == lines[0].last &&
            lines[0].first[0] && lines[0].first[1], lines.empty() ? 0 : lines[0].line, std::string("map:") + pair + " must be one 'T7 <-> T8' pair");
    }
    for (const char* id : {"t8_to_t7_req_never_true", "t7_to_t8_req_never_true"})
        check(rules.values.at(id) != 0, 0, std::string(id) + " cannot be 0");
    for (auto [offset, value] : rules.t8MoveDefaults) check(offset % 4 == 0 && offset < T8MoveSize, 0, "field:t7_to_t8_move offset outside the 0x448-byte move");
    std::set<std::pair<int, uint32_t>> seen;
    std::set<std::pair<std::string, uint32_t>> fixed;
    for (auto& t : rules.transforms) {
        CheckInputs(t);
        for (const auto& n : t.nodes) check(n.op != Transform::Node::Lookup || rules.tables.count(n.table), t.line, "unknown table '" + n.table + "'");
        for (const auto& n : t.nodes) check(n.op != Transform::Node::Setting || DefaultSettings().count(n.table), t.line, "unknown setting '" + n.table + "'");
        if (t.guard) {
            check(t.source >= 0x8000, t.line, "t8_to_t7_guard names a property row (0x8000 and up) of a requirement list");
            check(seen.insert({4, t.source}).second, t.line, "two guards for one t8_to_t7_guard id");
            bool id = false, v = false;
            for (auto [destination, root] : t.sets) { (void)root; id |= destination == IdDestination; v |= destination == -1; }
            check(id && v, t.line, "t8_to_t7_guard sets id = EXPR and v = EXPR");
            continue;
        }
        if (!t.fix.empty()) {
            const auto& kind = t.fix;
            check(std::any_of(std::begin(FixKinds), std::end(FixKinds), [&](const char* k) { return kind == k; }), t.line,
                "unknown t8_to_t7_fix kind '" + kind + "'");
            check(fixed.insert({kind, t.source}).second, t.line, "two fixes for one t8_to_t7_fix:" + kind + " id");
            check(t.drop >= 0 || !t.sets.empty(), t.line, "t8_to_t7_fix sets nothing (drop = EXPR or v = EXPR)");
            if (kind == "move" || kind == "projectile") {
                const auto size = kind == "move" ? T7MoveSize : T7ProjectileSize;
                check(t.drop < 0 && !t.missDrop, t.line, "t8_to_t7_fix:" + kind + " fields cannot be dropped");
                // t.source <= size - 4, not t.source + 4 <= size: 0xFFFFFFFC + 4 wraps to 0 in 32 bits.
                check(t.source % 4 == 0 && t.source <= size - 4, t.line, "t8_to_t7_fix:" + kind + " offset outside the record");
                const auto* first = kind == "move" ? std::begin(T7MovePointers) : std::begin(T7ProjectilePointers);
                const auto* last = kind == "move" ? std::end(T7MovePointers) : std::end(T7ProjectilePointers);
                check(std::none_of(first, last, [&](uint32_t at) { return t.source >= at && t.source < at + 8; }), t.line,
                    "t8_to_t7_fix:" + kind + " offset is a reference to another table");
            }
            // List ends keep their meaning.
            check(kind != "prop" || t.source != 0, t.line, "property 0 ends a list and cannot be fixed");
            check(kind != "req" || t.source != 881, t.line, "requirement 881 ends a list and cannot be fixed");
            check(kind != "cancel" || t.source != 0x8000, t.line, "cancel 0x8000 ends a list and cannot be fixed");
            check(kind != "voice" || t.source < 0xff, t.line, "voiceclip category must be below 0xFF");
            continue;
        }
        if (t.condition) {
            check(seen.insert({t.direction + 2, t.source}).second, t.line, "two conditions for one requirement");
            check(t.source != 0 && t.source != (t.direction == ToT8 ? 881u : 1100u), t.line, "requirement 0 and the list end cannot have a condition");
            continue;
        }
        check(seen.insert({t.direction, t.source}).second, t.line, "two transforms for one property");
        check(t.source && t.target, t.line, "property 0 cannot be transformed");
        // List ends as for map:t7_t8_prop (T7 side: source of a t7_to_t8_prop, target of a t8_to_t7_prop). A
        // t8_to_t7_prop target 1100 is refused too: ToT7 drops a row whose T7 id is 0, 881 or 1100.
        const uint32_t t7Id = t.direction == ToT8 ? t.source : t.target, t8Id = t.direction == ToT8 ? t.target : t.source;
        check(!endsList(0, t7Id, t7Id) && !endsList(1, t8Id, t8Id) && (t.direction == ToT8 || t.target != 1100), t.line,
            "a property transform cannot read or write a list end (0, T7 881, retail 1100)");
        bool setsV = false;
        for (auto [destination, root] : t.sets) {
            (void)root;
            check(t.direction == ToT7 ? destination == -1 || destination == IdDestination : destination >= 0, t.line,
                t.direction == ToT7 ? "t8_to_t7_prop sets v and id only" : "t7_to_t8_prop sets p0..p4 only");
            setsV |= destination == -1;
        }
        check(t.direction == ToT8 || setsV, t.line, "t8_to_t7_prop must set v");
        for (const auto& n : t.nodes) check(n.op != Transform::Node::Lookup || rules.tables.count(n.table), t.line, "unknown table '" + n.table + "'");
    }
}
}

IdMap::Result IdMap::Find(Direction direction, uint32_t x, uint32_t& out) const {
    const int src = direction == ToT8 ? 0 : 1, tgt = 1 - src;
    for (const auto& l : lines) {
        if (!l.forward[direction] || x < l.first[src] || x > l.last[src]) continue;
        if (l.none[direction]) return Result::None;
        const uint64_t mapped = uint64_t(l.first[tgt]) + (x - l.first[src]);
        if (mapped > UINT32_MAX) return Result::None;
        out = static_cast<uint32_t>(mapped); return Result::Mapped;
    }
    return Result::NoRule;
}

uint32_t Rules::Value(const std::string& name) const { return values.at(name); }
bool Rules::InList(const std::string& name, uint32_t value) const {
    for (auto [first, last] : lists.at(name)) if (value >= first && value <= last) return true;
    return false;
}
const IdMap& Rules::Map(const std::string& name) const { return maps.at(name); }
std::array<uint32_t, 2> Rules::Pair(const std::string& name) const { return maps.at(name).lines.at(0).first; }

Rules::Applied Rules::ToT8(uint32_t id, uint32_t v, uint32_t& target, std::array<uint32_t, 5>& p, const Settings& settings) const {
    for (const auto& t : transforms) {
        if (t.condition || t.guard || !t.fix.empty() || t.direction != ConversionRules::ToT8 || t.source != id) continue;
        std::array<uint32_t, 5> out{};
        try { for (auto [destination, root] : t.sets) out[static_cast<size_t>(destination)] = Evaluate(*this, t, root, v, {}, settings); }
        catch (const Miss&) { return t.missDrop ? Applied::Drop : Applied::None; }
        target = t.target; p = out; return Applied::Mapped;
    }
    return Applied::None;
}
Rules::Applied Rules::ToT7(uint32_t id, const std::array<uint32_t, 5>& p, uint32_t& target, uint32_t& v, const Settings& settings) const {
    for (const auto& t : transforms) {
        if (t.condition || t.guard || !t.fix.empty() || t.direction != ConversionRules::ToT7 || t.source != id) continue;
        uint32_t out = 0, chosen = t.target;
        try {
            for (auto [destination, root] : t.sets) {
                if (destination == IdDestination) chosen = Evaluate(*this, t, root, 0, p, settings);
                else out = Evaluate(*this, t, root, 0, p, settings);
            }
        }
        catch (const Miss&) { return t.missDrop ? Applied::Drop : Applied::None; }
        // A computed id never ends a list: 0 (properties), 881 (T7 requirements) or 1100 (retail requirements).
        if (chosen == 0 || chosen == 881 || chosen == 1100) return Applied::Drop;
        target = chosen; v = out; return Applied::Mapped;
    }
    return Applied::None;
}

const Settings& DefaultSettings() {
    // Must match ImportProfile's defaults (checked by tests): versus story flag, not in Heat, flags clear.
    static const Settings defaults{{"story_flag", 0}, {"in_heat", 0}, {"flags_clear", 1}};
    return defaults;
}

std::optional<bool> Rules::RequirementHolds(Direction direction, uint32_t id, uint32_t v, const std::array<uint32_t, 5>& p,
    const Settings& settings) const {
    for (const auto& t : transforms) {
        if (!t.condition || t.direction != direction || t.source != id) continue;
        try { return Evaluate(*this, t, t.sets.front().second, v, p, settings) != 0; }
        catch (const Miss&) { return false; }
    }
    return std::nullopt;
}

FixResult Rules::Fix(std::string_view kind, uint32_t id, uint32_t v, const std::array<uint32_t, 5>& p, const Settings& settings) const {
    for (const auto& t : transforms) {
        if (t.fix != kind || t.source != id) continue;
        try {
            if (t.drop >= 0 && Evaluate(*this, t, t.drop, v, p, settings)) return {true, true, v};
            uint32_t out = v;
            for (auto [destination, root] : t.sets) { (void)destination; out = Evaluate(*this, t, root, v, p, settings); }
            return {false, out != v, out};
        } catch (const Miss&) { return {t.missDrop, t.missDrop, v}; }
    }
    return {false, false, v};
}
std::optional<std::pair<uint32_t, uint32_t>> Rules::Guard(uint32_t id, uint32_t v, const Settings& settings) const {
    for (const auto& t : transforms) {
        if (!t.guard || t.source != id) continue;
        try {
            if (t.when >= 0 && !Evaluate(*this, t, t.when, v, {}, settings)) return std::nullopt;
            uint32_t row = 0, value = 0;
            for (auto [destination, root] : t.sets) (destination == IdDestination ? row : value) = Evaluate(*this, t, root, v, {}, settings);
            if (row == 0 || row == 881 || row >= 0x8000) return std::nullopt; // not a condition
            return std::pair<uint32_t, uint32_t>{row, value};
        } catch (const Miss&) { return std::nullopt; }
    }
    return std::nullopt;
}
std::vector<uint32_t> Rules::FixIds(std::string_view kind) const {
    std::vector<uint32_t> ids;
    for (const auto& t : transforms) if (t.fix == kind) ids.push_back(t.source);
    return ids;
}

Rules Parse(std::string_view text) {
    if (text.size() > TextLimit) throw Error(0, "file exceeds 4 MiB");
    Rules rules;
    std::istringstream in{std::string(text)};
    std::string raw; uint32_t number = 0;
    uint64_t tableEntries = 0;
    while (std::getline(in, raw)) {
        if (++number > LineLimit) throw Error(number, "too many lines");
        if (const auto hash = raw.find('#'); hash != std::string::npos) raw.resize(hash);
        const auto line = Trim(raw);
        if (line.empty()) continue;
        const auto colon = line.find(':');
        if (colon == std::string::npos) throw Error(number, "expected PREFIX:NAME");
        const auto prefix = line.substr(0, colon), rest = line.substr(colon + 1);
        if (prefix == "t7_to_t8_req" || prefix == "t8_to_t7_req") {
            Transform t; t.direction = prefix == "t7_to_t8_req" ? ToT8 : ToT7; t.line = number; t.condition = true;
            const auto eq = rest.find('=');
            if (eq == std::string::npos) throw Error(number, "condition needs 'ID = EXPR'");
            t.source = NumberOrThrow(number, Trim(rest.substr(0, eq)));
            const auto expr = Trim(rest.substr(eq + 1));
            Parser parser{t, number, expr};
            const int root = parser.Compare();
            parser.Space();
            if (parser.at != expr.size() || expr.empty()) throw Error(number, "unexpected '" + expr.substr(parser.at) + "'");
            t.sets.push_back({-2, root});
            rules.transforms.push_back(std::move(t));
            continue;
        }
        if (prefix == "t8_to_t7_guard") {
            // "t8_to_t7_guard:ID  when = EXPR ; id = EXPR ; v = EXPR" (when optional, ';'-separated)
            Transform t; t.direction = ToT7; t.line = number; t.guard = true;
            const auto space = rest.find_first_of(" 	");
            if (space == std::string::npos) throw Error(number, "t8_to_t7_guard has no assignment");
            t.source = NumberOrThrow(number, Trim(rest.substr(0, space)));
            for (const auto& assignment : Split(rest.substr(space), ';')) {
                if (assignment.empty()) continue;
                const auto eq = assignment.find('=');
                if (eq == std::string::npos) throw Error(number, "expected when = EXPR, id = EXPR or v = EXPR");
                const auto destination = Trim(assignment.substr(0, eq)), expr = Trim(assignment.substr(eq + 1));
                if (destination != "when" && destination != "id" && destination != "v") throw Error(number, "t8_to_t7_guard sets when, id or v");
                const int index = destination == "id" ? IdDestination : -1;
                if (destination == "when" ? t.when >= 0 : std::any_of(t.sets.begin(), t.sets.end(), [&](auto s) { return s.first == index; }))
                    throw Error(number, destination + " given twice");
                Parser parser{t, number, expr};
                const int root = parser.Compare();
                parser.Space();
                if (parser.at != expr.size() || expr.empty()) throw Error(number, "unexpected '" + expr.substr(parser.at) + "'");
                if (destination == "when") t.when = root; else t.sets.push_back({index, root});
            }
            rules.transforms.push_back(std::move(t));
            continue;
        }
        if (prefix == "t8_to_t7_fix") {
            // "t8_to_t7_fix:KIND:ID  drop = EXPR ; v = EXPR ; miss = drop" (any of the three, ';'-separated)
            Transform t; t.direction = ToT7; t.line = number;
            const auto split = rest.find(':');
            if (split == std::string::npos) throw Error(number, "t8_to_t7_fix:KIND:ID expected");
            t.fix = Trim(rest.substr(0, split));
            const auto tail = Trim(rest.substr(split + 1));
            const auto space = tail.find_first_of(" \t");
            if (space == std::string::npos) throw Error(number, "t8_to_t7_fix has no assignment");
            t.source = NumberOrThrow(number, tail.substr(0, space));
            for (const auto& assignment : Split(tail.substr(space), ';')) {
                if (assignment.empty()) continue;
                const auto eq = assignment.find('=');
                if (eq == std::string::npos) throw Error(number, "expected drop = EXPR, v = EXPR or miss = drop");
                const auto destination = Trim(assignment.substr(0, eq)), expr = Trim(assignment.substr(eq + 1));
                if (destination == "miss") { if (expr != "drop") throw Error(number, "miss = drop expected"); t.missDrop = true; continue; }
                if (destination != "v" && destination != "drop") throw Error(number, "t8_to_t7_fix sets drop or v");
                if (destination == "drop" ? t.drop >= 0 : !t.sets.empty()) throw Error(number, destination + " given twice");
                Parser parser{t, number, expr};
                const int root = parser.Compare();
                parser.Space();
                if (parser.at != expr.size() || expr.empty()) throw Error(number, "unexpected '" + expr.substr(parser.at) + "'");
                if (destination == "drop") t.drop = root; else t.sets.push_back({-1, root});
            }
            rules.transforms.push_back(std::move(t));
            continue;
        }
        if (prefix == "t7_to_t8_prop" || prefix == "t8_to_t7_prop") {
            Transform t; t.direction = prefix == "t7_to_t8_prop" ? ToT8 : ToT7; t.line = number;
            const auto arrow = rest.find("->");
            if (arrow == std::string::npos) throw Error(number, "transform needs 'SOURCE -> TARGET'");
            t.source = NumberOrThrow(number, Trim(rest.substr(0, arrow)));
            auto tail = Trim(rest.substr(arrow + 2));
            const auto space = tail.find_first_of(" \t");
            if (space == std::string::npos) throw Error(number, "transform has no assignment");
            t.target = NumberOrThrow(number, tail.substr(0, space));
            for (const auto& assignment : Split(tail.substr(space), ';')) {
                if (assignment.empty()) continue;
                const auto eq = assignment.find('=');
                if (eq == std::string::npos) throw Error(number, "expected DEST = EXPR");
                const auto destination = Trim(assignment.substr(0, eq)), expr = Trim(assignment.substr(eq + 1));
                if (destination == "miss") { if (expr != "drop") throw Error(number, "miss = drop expected"); t.missDrop = true; continue; }
                int index;
                if (destination == "v") index = -1;
                else if (destination == "id") index = IdDestination; // t8_to_t7_prop: the T7 id, chosen from p0..p4
                else if (destination.size() == 2 && destination[0] == 'p' && destination[1] >= '0' && destination[1] <= '4') index = destination[1] - '0';
                else throw Error(number, "destination must be v, id or p0..p4");
                Parser parser{t, number, expr};
                const int root = parser.Compare();
                parser.Space();
                if (parser.at != expr.size()) throw Error(number, "unexpected '" + expr.substr(parser.at) + "'");
                t.sets.push_back({index, root});
            }
            if (t.sets.empty()) throw Error(number, "transform sets nothing");
            rules.transforms.push_back(std::move(t));
            continue;
        }
        const auto eq = rest.find('=');
        if (eq == std::string::npos) throw Error(number, "expected NAME = VALUE");
        const auto name = Trim(rest.substr(0, eq)), body = Trim(rest.substr(eq + 1));
        if (name.empty()) throw Error(number, "empty name");
        if (prefix == "val") {
            if (rules.values.count(name)) throw Error(number, "val:" + name + " given twice");
            rules.values[name] = NumberOrThrow(number, body);
        } else if (prefix == "list") {
            auto& list = rules.lists[name];
            for (const auto& item : Split(body, ',')) {
                const auto span = ParseSpan(number, item, true, false);
                list.push_back({span.first, span.last});
            }
        } else if (prefix == "map") {
            ParseMapLine(rules, number, name, body);
        } else if (prefix == "table") {
            // KEY:VALUE, or FIRST..LAST:VALUE for FIRST+i -> VALUE+i.
            auto& table = rules.tables[name];
            for (const auto& item : Split(body, ',')) {
                const auto at = item.find(':');
                if (at == std::string::npos) throw Error(number, "table entries are KEY:VALUE or FIRST..LAST:VALUE");
                const auto keys = ParseSpan(number, Trim(item.substr(0, at)), false, false);
                const auto value = NumberOrThrow(number, Trim(item.substr(at + 1)));
                if (keys.last - keys.first > 0xFFFF) throw Error(number, "table range longer than 65536 keys");
                if (uint64_t(value) + (keys.last - keys.first) > UINT32_MAX) throw Error(number, "table range value overflows");
                tableEntries += uint64_t(keys.last - keys.first) + 1;
                if (tableEntries > TableEntryLimit) throw Error(number, "tables hold more than 1048576 keys in all");
                for (uint64_t key = keys.first; key <= keys.last; ++key)
                    if (!table.emplace(static_cast<uint32_t>(key), static_cast<uint32_t>(value + (key - keys.first))).second)
                        throw Error(number, "table key given twice");
            }
        } else if (prefix == "field" && name == "t7_to_t8_move") {
            // "OFFSET[..LAST] = VALUE" (the name part holds "t8_move = OFFSET..LAST", the value after the second '=')
            const auto second = body.find('=');
            if (second == std::string::npos) throw Error(number, "field:t7_to_t8_move = OFFSET[..LAST] = VALUE expected");
            const auto span = ParseSpan(number, Trim(body.substr(0, second)), false, false);
            const auto value = NumberOrThrow(number, Trim(body.substr(second + 1)));
            if ((span.last - span.first) % 4) throw Error(number, "field range must step by 4");
            // Checked before expanding the range: 0..0xFFFFFFFC would otherwise ask for a billion entries.
            if (span.first % 4 || span.last >= T8MoveSize) throw Error(number, "field:t7_to_t8_move offset outside the 0x448-byte move");
            for (uint64_t offset = span.first; offset <= span.last; offset += 4) rules.t8MoveDefaults.push_back({static_cast<uint32_t>(offset), value});
        } else {
            throw Error(number, "unknown prefix '" + prefix + ":" + name + "'");
        }
    }
    Validate(rules);
    return rules;
}

std::string Section(std::string_view addresses) {
    const auto begin = addresses.find(SectionBegin);
    if (begin == std::string_view::npos) return std::string(addresses); // a bare rules file (Rules/t7_t8_conversion_rules.ini)
    const auto body = addresses.find('\n', begin);
    const auto end = addresses.find(SectionEnd, begin);
    if (body == std::string_view::npos || end == std::string_view::npos || end < body) throw Error(0, "conversion rules section has no end marker");
    return std::string(addresses.substr(body + 1, end - body - 1));
}
std::string_view EmbeddedText() {
    static const std::string text = Section({game_addresses_ini, game_addresses_ini_size ? game_addresses_ini_size - 1 : 0});
    return text;
}
const Rules& Embedded() {
    static const Rules rules = Parse(EmbeddedText());
    return rules;
}

namespace {
struct Active { Rules rules; Status status; };
const Active& Load() {
    static const Active active = [] {
        Active result{Embedded(), {"built-in", "", Embedded().version, Embedded().version}};
        std::error_code error;
        if (!std::filesystem::exists(GAME_ADDRESSES_FILE, error)) return result;
        try {
            std::ifstream file(GAME_ADDRESSES_FILE, std::ios::binary);
            std::stringstream text; text << file.rdbuf();
            auto downloaded = Parse(Section(text.str()));
            // An app update can bring newer built-in rules than an older downloaded game_addresses.ini.
            if (downloaded.version < Embedded().version) {
                result.status.error = std::string(GAME_ADDRESSES_FILE) + " holds conversion rules version " + std::to_string(downloaded.version) +
                    ", older than the built-in " + std::to_string(Embedded().version);
                return result;
            }
            result.status = {GAME_ADDRESSES_FILE, "", downloaded.version, Embedded().version};
            result.rules = std::move(downloaded);
        } catch (const std::exception& e) {
            result.status.error = e.what(); // keep the built-in rules
        }
        return result;
    }();
    return active;
}
}
const Rules& Current() { return Load().rules; }
Status CurrentStatus() { return Load().status; }
}
