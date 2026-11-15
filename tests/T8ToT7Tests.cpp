// Retail T8 -> T7 conversion (T8ToT7) and packed PANM track decoding (RetailPanm::DecodePackedTrack).
#include "T8ToT7.hpp"
#include "ConversionRules.hpp"
#include "ImportProfile.hpp"
#include "Importer_t8/RetailImport.hpp"
#include "T7ToRetailT8.hpp"
#include "RetailNames.hpp"
#include "RetailPanm.hpp"
#include "RetailT8AnimToT7.hpp"
#include "MovesetFileConverter.hpp"
#include "Importer_t7/Importer_t7_Utils.hpp"
#include "Extractor_t8/RetailSnapshot.hpp"
#include "GameIDs.hpp"
#include "Structs_t7.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<Byte> SemanticBodyFixture();
namespace {
void Check(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
template<class F> void Reject(F&& callback, const char* what) {
    bool failed = false;
    try { callback(); } catch (const std::exception&) { failed = true; }
    Check(failed, what);
}
template<class T> void Put(std::vector<Byte>& bytes, size_t at, T value) { memcpy(bytes.data() + at, &value, sizeof(value)); }
template<class T> T At(std::span<const Byte> bytes, size_t at) { T value; memcpy(&value, bytes.data() + at, sizeof(value)); return value; }

// A packed track: scale and quaternion Y constant, quaternion X (8 bits), Z (4 bits), translation Y (10 bits) and the
// W sign bit, three frames.
struct PackedFrame { uint32_t x, z, y, sign; };
std::vector<Byte> PackedTrack(const std::vector<PackedFrame>& frames, uint16_t version = 4, uint16_t bitsPerFrame = 23) {
    std::vector<Byte> b(204);
    Put<uint16_t>(b, 0, version); Put<uint16_t>(b, 2, 6); Put<uint16_t>(b, 4, 160); Put<uint16_t>(b, 6, bitsPerFrame);
    Put<uint32_t>(b, 8, 204); Put<uint32_t>(b, 12, static_cast<uint32_t>(frames.size()));
    const float channels[9][3] = {{1, 1, 16}, {1, 1, 16}, {1, 1, 16}, {-0.5f, 0.5f, 8}, {0, 0, 16}, {0, 0.25f, 4},
                                  {0, 0, 0}, {10, 20, 10}, {5, 5, 0}};
    for (size_t c = 0; c < 9; ++c) {
        Put<float>(b, 16 + c * 16, channels[c][0]); Put<float>(b, 20 + c * 16, channels[c][1]);
        Put<uint32_t>(b, 24 + c * 16, static_cast<uint32_t>(channels[c][2]));
    }
    std::vector<bool> bits;
    auto push = [&](uint32_t value, int width) { for (int i = 0; i < width; ++i) bits.push_back((value >> i) & 1); };
    for (const auto& f : frames) { push(f.x, 8); push(f.z, 4); push(f.y, 10); push(f.sign, 1); }
    b.resize(204 + (bits.size() + 7) / 8);
    for (size_t i = 0; i < bits.size(); ++i) if (bits[i]) b[204 + i / 8] |= Byte(1u << (i % 8));
    return b;
}
void RunPackedTrackTests() {
    const std::vector<PackedFrame> frames = {{0, 15, 0, 0}, {255, 0, 1023, 1}, {128, 8, 511, 0}};
    const auto samples = RetailPanm::DecodePackedTrack(PackedTrack(frames), 3);
    Check(samples.size() == 3, "Packed track sample count");
    for (size_t i = 0; i < 3; ++i) {
        const auto& s = samples[i];
        const double x = -0.5 + frames[i].x / 255.0, z = 0.25 * frames[i].z / 15.0, y = 10 + 10.0 * frames[i].y / 1023.0;
        const double w = std::sqrt(1 - x * x - z * z) * (frames[i].sign ? -1 : 1);
        Check(s.scale == std::array<float, 3>{1, 1, 1}, "Packed constant scale");
        Check(std::fabs(s.rotation[0] - x) < 1e-6 && s.rotation[1] == 0 && std::fabs(s.rotation[2] - z) < 1e-6 &&
            std::fabs(s.rotation[3] - w) < 1e-6, "Packed quaternion frame " + std::to_string(i));
        Check(s.translation[0] == 0 && std::fabs(s.translation[1] - y) < 1e-4 && s.translation[2] == 5,
            "Packed translation frame " + std::to_string(i));
    }
    Reject([&] { RetailPanm::DecodePackedTrack(PackedTrack(frames, 5), 3); }, "Unknown packed track version accepted");
    Reject([&] { RetailPanm::DecodePackedTrack(PackedTrack(frames, 4, 21), 3); }, "Packed bit count mismatch accepted");
    Reject([&] { RetailPanm::DecodePackedTrack(PackedTrack(frames), 4); }, "Packed sample count mismatch accepted");
    auto truncated = PackedTrack(frames); truncated.pop_back();
    Reject([&] { RetailPanm::DecodePackedTrack(truncated, 3); }, "Truncated packed track accepted");
}
}

// t7_t8_conversion_rules.ini: the embedded defaults parse and hold the converters' rules; unsafe or incomplete files are refused.
void RunConversionRulesTests() {
    namespace C = ConversionRules;
    const auto& rules = C::Embedded();
    Check(rules.version >= 1 && rules.format == C::SupportedFormat, "Embedded conversion rules version/format");
    Check(&C::Current() == &C::Current(), "Current rules are not stable");
    const auto& cmd = rules.Map("t7_t8_cmd");
    Check(cmd.Map(C::ToT8, 0x8003) == 0x8005u && cmd.Map(C::ToT8, 0x800a) == 0x800cu && cmd.Map(C::ToT8, 0x800b) == 0x8012u &&
        cmd.Map(C::ToT8, 0x8100) == 0x8107u, "Forward special commands");
    Check(cmd.Map(C::ToT7, 0x8005) == 0x8003u && !cmd.Map(C::ToT7, 0x8007) && !cmd.Map(C::ToT7, 0x8003) && cmd.Map(C::ToT7, 0x8012) == 0x800bu,
        "Retail special commands (0x8007 has no T7 command)");
    Check(rules.Map("t7_t8_alias_slot").Map(C::ToT8, 55) == 59u && !rules.Map("t7_t8_alias_slot").Map(C::ToT7, 31) &&
        rules.Map("t7_t8_alias_slot").Map(C::ToT7, 40) == 37u, "Alias slots");
    Check(rules.Map("t7_t8_character").Map(C::ToT8, 9) == 8u && rules.Map("t7_t8_character").Map(C::ToT7, 39) == 56u, "Characters");
    uint32_t id, v; std::array<uint32_t, 5> p{};
    Check(rules.ToT8(0x84C3, 0x07000123, id, p) == C::Rules::Applied::Mapped && id == 0x87EF && p[0] == 7 && p[1] == 0 && p[2] == 0x123,
        "Sound transform to T8");
    Check(rules.ToT7(0x87EF, {7, 0, 0x123, 0, 0}, id, v) == C::Rules::Applied::Mapped && id == 0x84C3 && v == 0x07000123, "Sound transform to T7");
    Check(rules.ToT8(0x8409, 0xC, id, p) == C::Rules::Applied::Mapped && p[0] == 9 && rules.ToT8(0x8409, 6, id, p) == C::Rules::Applied::None,
        "Battle flag transform to T8 (a miss falls through)");
    Check(rules.ToT7(0x8552, {9}, id, v) == C::Rules::Applied::Mapped && v == 0xC && rules.ToT7(0x8552, {6}, id, v) == C::Rules::Applied::Drop,
        "Battle flag transform to T7 (a miss drops)");
    // Trail VFX (t8_to_t7_fix:prop:0x8036, effect id << 8 | attach point): ids T7 has (0..31) are kept, a retail-only id
    // drops the row (T8 Jin's stance up+1 0x790b crashed T7).
    Check(!rules.Fix("prop", 0x8036, 0x1e).drop && !rules.Fix("prop", 0x8036, 0x1f0b).drop && !rules.Fix("prop", 0x8036, 0x1f0b).changed,
        "Trail VFX T7 has");
    Check(rules.Fix("prop", 0x8036, 0x790b).drop && rules.Fix("prop", 0x8036, 0x2000).drop && rules.Fix("prop", 0x8036, 0x3346).drop,
        "Retail-only trail VFX id reaches T7");
    Check(!rules.Fix("prop", 0x8035, 0x790b).drop && rules.Fix("prop", 0x8035, 0x790b).v == 0x790b, "A fix reached another property");
    // Requirements whose T7 id comes from a retail parameter (id = EXPR): the throw break button (0x87 p0 5 / 6 = T7
    // 0x83 / 0x84, never true before) and opponent facing (0x1FD p0 0xB = 0x5C); other parameters stay unresolved.
    Check(rules.ToT7(0x87, {5}, id, v) == C::Rules::Applied::Mapped && id == 0x83 && v == 0 &&
        rules.ToT7(0x87, {6}, id, v) == C::Rules::Applied::Mapped && id == 0x84 && rules.ToT7(0x87, {7}, id, v) == C::Rules::Applied::Drop,
        "Throw break button requirement");
    Check(rules.ToT7(0x1FD, {0xB}, id, v) == C::Rules::Applied::Mapped && id == 0x5C && rules.ToT7(0x1FD, {3}, id, v) == C::Rules::Applied::Drop,
        "Opponent facing requirement");
    // Second player sound (0x84C5 <-> 0x87F1) splits like the other three.
    Check(rules.ToT8(0x84C5, 0x07000123, id, p) == C::Rules::Applied::Mapped && id == 0x87F1 && p[0] == 7 && p[1] == 0 && p[2] == 0x123 &&
        rules.ToT7(0x87F1, {7, 0, 0x123, 0, 0}, id, v) == C::Rules::Applied::Mapped && id == 0x84C5 && v == 0x07000123, "Sound 0x84C5 transform");
    // Wall-hit counter checks: T7's threshold becomes retail {counter 1, threshold}; retail counter 2 has no T7 row.
    for (auto [t7, t8] : {std::pair{283u, 287u}, {320u, 325u}, {358u, 364u}}) {
        p = {};
        Check(rules.ToT8(t7, 3, id, p) == C::Rules::Applied::Mapped && id == t8 && p[0] == 1 && p[1] == 3, "Wall-hit counter to T8");
        Check(rules.ToT7(t8, {1, 3, 0, 0, 0}, id, v) == C::Rules::Applied::Mapped && id == t7 && v == 3 &&
            rules.ToT7(t8, {2, 3, 0, 0, 0}, id, v) == C::Rules::Applied::Drop, "Wall-hit counter to T7");
    }
    Check(rules.t8MoveDefaults.size() == 50, "Retail move field defaults");
    // Requirement conditions: 777 Story Flag == X holds only for versus (0; 4 is Devil Jin's story chapter); flag
    // comparisons 476/439 with 0 hold (flags clear in normal play).
    Check(rules.RequirementHolds(C::ToT7, 777, 0, {0}) == true && rules.RequirementHolds(C::ToT7, 777, 0, {4}) == false &&
        rules.RequirementHolds(C::ToT7, 476, 0, {16, 0}) == true && rules.RequirementHolds(C::ToT7, 476, 0, {16, 1}) == false &&
        rules.RequirementHolds(C::ToT7, 439, 0, {0xF0000, 0}) == true && !rules.RequirementHolds(C::ToT7, 35, 0, {4}) &&
        !rules.RequirementHolds(C::ToT8, 777, 4, {}), "Story flag and flag-clear conditions");

    // Every edit below must be refused as a whole.
    const std::string base(C::EmbeddedText());
    auto refused = [&](const std::string& text, const char* what) {
        bool failed = false;
        try { C::Parse(text); } catch (const std::runtime_error&) { failed = true; }
        Check(failed, std::string("Unsafe conversion rules accepted: ") + what);
    };
    auto replaced = [&](const std::string& from, const std::string& to) {
        auto text = base; const auto at = text.find(from);
        Check(at != std::string::npos, "Test rule line not found: " + from);
        return text.replace(at, from.size(), to);
    };
    refused(replaced("val:t7_t8_rules_format = 1", "val:t7_t8_rules_format = 2"), "unknown format");
    refused(replaced("val:t7_t8_walk_cancel_t8_frame = 5", ""), "missing value");
    refused(replaced("map:t7_t8_req = 881 <-> 1100", "map:t7_t8_req = 882 <-> 1100"), "remapped list terminator");
    refused(base + "map:t7_t8_req = 5 -> 1100\n", "second source for the terminator");
    refused(base + "map:t7_t8_cmd = 0x7000 <-> 0x7000\n", "command below 0x8000");
    refused(base + "map:t7_t8_cmd = 0x9000 <-> 0x8000\n", "remapped cancel list end");
    refused(base + "map:t7_t8_cmd = 0x8001 <-> 0x9001\n", "two rules for one command");
    refused(replaced("map:t7_t8_alias_slot = 55 <-> 59", ""), "T7 alias slot without T8 slot");
    refused(base + "map:t7_t8_prop = 0 <-> 5\n", "mapped property list end");
    refused(base + "t8_to_t7_prop:0x87EF -> 0x84C3  v = p0\n", "two transforms for one property");
    refused(base + "t7_to_t8_prop:0x1234 -> 0x5678  p0 = p1\n", "forward transform reading retail parameters");
    refused(base + "t7_to_t8_prop:0x1234 -> 0x5678  p0 = lookup(nothing, v)\n", "unknown lookup table");
    refused(base + "field:t7_to_t8_move = 0x449 = 1\n", "move field outside the record");
    refused(base + "val:surprise = 1\n", "unknown value");
    refused(base + "map:surprise = 1 <-> 1\n", "unknown map");
    refused(base + "bogus line\n", "malformed line");
    refused(base + "t8_to_t7_req:777 = p0 == 3\n", "two conditions for one requirement");
    refused(base + "t8_to_t7_req:801 = v == 1\n", "retail condition reading the T7 value");
    refused(base + "t8_to_t7_req:1100 = p0 == 0\n", "condition on the list end");
    refused(base + "t7_to_t8_req:558 =\n", "empty condition");
    // Malformed input is refused before it can exhaust memory or the stack.
    refused(base + "field:t7_to_t8_move = 0..0xFFFFFFFC = 0\n", "move field range expanded before its bounds were checked");
    refused(base + "t7_to_t8_prop:0x1234 -> 0x5678  p0 = " + std::string(200000, '(') + "v" + std::string(200000, ')') + "\n", "deeply nested expression");
    {
        std::string chain = "v";
        for (int i = 0; i < 200000; ++i) chain += "+1";
        refused(base + "t7_to_t8_prop:0x1234 -> 0x5678  p0 = " + chain + "\n", "long arithmetic chain");
    }
    {   // A forward transform sets all five retail parameters.
        auto five = C::Parse(base + "t7_to_t8_prop:0x1234 -> 0x5678  p0 = 1 ; p1 = 2 ; p2 = 3 ; p3 = v ; p4 = v + 1\n");
        Check(five.ToT8(0x1234, 9, id, p) == C::Rules::Applied::Mapped && id == 0x5678 && p == std::array<uint32_t, 5>{1, 2, 3, 9, 10},
            "Forward transform parameters p3/p4");
    }
    {   // t8_to_t7_fix on every record kind, the ordered comparisons and table ranges.
        auto fixes = C::Parse(base +
            "t8_to_t7_fix:req:0x30  drop = v >= 5 ; v = v + 1\n"
            "t8_to_t7_fix:voice:7  v = lookup(clip_ids, v) ; miss = drop\n"
            "t8_to_t7_fix:cancel:0x8003  drop = v < 3\n"
            "t8_to_t7_fix:move:0x98  v = v & 0xFFFF\n"
            "t8_to_t7_fix:projectile:0x74  v = 1\n"
            "t8_to_t7_fix:prop:0x84C4  drop = p0 > 0x20 ; v = v | (p1 <= 2)\n"
            "table:clip_ids = 0x10..0x12:0x20, 0x40:0x41\n");
        Check(fixes.Fix("req", 0x30, 5).drop && !fixes.Fix("req", 0x30, 4).drop && fixes.Fix("req", 0x30, 4).v == 5 && fixes.Fix("req", 0x30, 4).changed,
            "t8_to_t7_fix:req drop and rewrite");
        Check(fixes.Fix("voice", 7, 0x11).v == 0x21 && fixes.Fix("voice", 7, 0x40).v == 0x41 && fixes.Fix("voice", 7, 0x13).drop,
            "t8_to_t7_fix:voice with a table range and miss = drop");
        Check(fixes.Fix("cancel", 0x8003, 2).drop && !fixes.Fix("cancel", 0x8003, 3).drop, "t8_to_t7_fix:cancel");
        Check(fixes.Fix("move", 0x98, 0x12345678).v == 0x5678 && fixes.FixIds("move") == std::vector<uint32_t>{0x98} &&
            fixes.Fix("projectile", 0x74, 9).v == 1, "t8_to_t7_fix field kinds");
        Check(fixes.Fix("prop", 0x84C4, 0x10, {0x21}).drop && fixes.Fix("prop", 0x84C4, 0x10, {0x20, 2}).v == 0x11 &&
            fixes.Fix("prop", 0x84C4, 0x10, {0x20, 3}).v == 0x10, "t8_to_t7_fix:prop reads retail parameters");
        // "<<" / ">>" stay shifts next to "<" / ">".
        Check(C::Parse(base + "t8_to_t7_fix:move:0x9C  v = (v << 4 >> 4) > 2\n").Fix("move", 0x9C, 3).v == 1, "Shifts next to comparisons");
    }
    {   // t8_to_t7_guard: the condition row for a condition-less list holding the named property row.
        using Row = std::optional<std::pair<uint32_t, uint32_t>>;
        const auto& embedded = C::Embedded();
        Check(embedded.Guard(0x8106, 0x1b0000) == Row({284, 0x1b0001}) && !embedded.Guard(0x8106, 0x1b02be) &&
            !embedded.Guard(0x8107, 0x1b0000), "t8_to_t7_guard:0x8106 (flag reset to 0)");
        auto guards = C::Parse(base + "t8_to_t7_guard:0x8200  id = v ; v = 7\n");
        Check(guards.Guard(0x8200, 5) == Row({5, 7}) && !guards.Guard(0x8200, 881) && !guards.Guard(0x8200, 0x8001) &&
            !guards.Guard(0x8200, 0), "t8_to_t7_guard without when; computed ids that are no condition");
        uint32_t target, value;
        Check(guards.ToT7(0x8200, {}, target, value) == C::Rules::Applied::None, "a guard is not a property transform");
    }
    refused(base + "t8_to_t7_guard:0x10  id = 284 ; v = 1\n", "guard on a requirement id below 0x8000");
    refused(base + "t8_to_t7_guard:0x8106  id = 284 ; v = 2\n", "two guards for one id");
    refused(base + "t8_to_t7_guard:0x8200  id = 284\n", "guard without v");
    refused(base + "t8_to_t7_guard:0x8200  v = 1\n", "guard without id");
    refused(base + "t8_to_t7_guard:0x8200  id = 284 ; v = p0\n", "guard reading retail parameters");
    refused(base + "t8_to_t7_guard:0x8200  id = 284 ; v = 1 ; v = 2\n", "guard setting v twice");
    refused(base + "t8_to_t7_guard:0x8200  id = 284 ; v = 1 ; p0 = 2\n", "guard setting a parameter");
    refused(base + "t8_to_t7_fix:bogus:1  v = 1\n", "unknown fix kind");
    refused(base + "t8_to_t7_fix:prop:0x8036  drop = v > 1\n", "two fixes for one id");
    refused(base + "t8_to_t7_fix:prop:0  drop = 1\n", "fix on the property list end");
    refused(base + "t8_to_t7_fix:req:881  drop = 1\n", "fix on the requirement list end");
    refused(base + "t8_to_t7_fix:cancel:0x8000  drop = 1\n", "fix on the cancel list end");
    refused(base + "t8_to_t7_fix:move:0x80  v = 0\n", "fix on a move reference field");
    refused(base + "t8_to_t7_fix:move:0x84  v = 0\n", "fix on the high half of a move reference field");
    refused(base + "t8_to_t7_fix:projectile:0x60  v = 0\n", "fix on a projectile reference field");
    refused(base + "t8_to_t7_fix:move:0xB0  v = 0\n", "fix outside the move");
    refused(base + "t8_to_t7_fix:move:0x9A  v = 0\n", "unaligned move fix");
    refused(base + "t8_to_t7_fix:move:0x98  drop = 1\n", "dropped move field");
    refused(base + "t8_to_t7_fix:voice:7  v = p0\n", "voice fix reading retail parameters");
    refused(base + "t8_to_t7_fix:voice:0xFF  drop = 1\n", "fix on the voiceclip list end");
    refused(base + "t8_to_t7_fix:req:5\n", "fix without assignment");
    refused(base + "t8_to_t7_fix:req:5  p0 = 1\n", "fix setting a retail parameter");
    refused(base + "t8_to_t7_fix:req:5  v = 1 ; v = 2\n", "fix setting v twice");
    refused(base + "t8_to_t7_fix:req:5  v = lookup(nothing, v)\n", "fix with an unknown table");
    refused(base + "table:range = 0..0x10000:0\n", "table range too long");
    refused(base + "table:range = 0..2:0xFFFFFFFF\n", "table range value overflow");
    refused(base + "table:range = 0..2:0, 1:5\n", "table range key given twice");
    refused(base + "t7_to_t8_prop:0x1234 -> 0x5678  p0 = v ; id = 7\n", "forward transform setting id");
    // Field fix offsets are bounded without 32-bit wrap-around (0xFFFFFFFC + 4 == 0).
    refused(base + "t8_to_t7_fix:move:0xFFFFFFFC  v = 0\n", "move fix offset wrapping around 32 bits");
    refused(base + "t8_to_t7_fix:projectile:0xFFFFFFFC  v = 0\n", "projectile fix offset wrapping around 32 bits");
    // Property transforms and maps also run on requirement rows and list ends: none may read or write 0 / 881 / 1100.
    refused(base + "t7_to_t8_prop:881 -> 5  p0 = v\n", "forward transform rewriting the T7 requirement list end");
    refused(base + "t8_to_t7_prop:1100 -> 5  v = p0\n", "reverse transform rewriting the retail requirement list end");
    refused(base + "t7_to_t8_prop:0x1234 -> 1100  p0 = v\n", "forward transform writing the retail list end");
    refused(base + "t8_to_t7_prop:0x1234 -> 881  v = p0\n", "reverse transform writing the T7 list end");
    refused(base + "t8_to_t7_prop:0x1234 -> 1100  v = p0\n", "reverse transform writing 1100");
    refused(base + "map:t7_t8_prop = 0x1234 -> 1100\n", "property map writing the retail list end");
    refused(base + "map:t7_t8_prop = 881 <- 0x1234\n", "property map writing the T7 list end");
    refused(base + "map:t7_t8_prop = 800..900 -> 0x9000..0x9064\n", "property map range covering the T7 list end");
    {   // Table keys are capped over the whole file, not only per range (17 x 65536 > 1048576).
        std::string tables = "table:many = ";
        for (uint32_t i = 0; i < 17; ++i)
            tables += (i ? ", " : "") + std::to_string(i * 0x10000) + ".." + std::to_string(i * 0x10000 + 0xFFFF) + ":0";
        refused(base + tables + "\n", "more table keys than the file-wide cap");
    }
    {   // Property transforms read the import profile through setting(NAME), as conditions do.
        auto profiled = C::Parse(base + "t7_to_t8_prop:0x1234 -> 0x5678  p0 = setting(story_flag)\n"
            "t8_to_t7_prop:0x1234 -> 0x5678  v = setting(in_heat) + p0\n");
        auto story = C::DefaultSettings(); story["story_flag"] = 4; story["in_heat"] = 1;
        Check(profiled.ToT8(0x1234, 0, id, p, story) == C::Rules::Applied::Mapped && p[0] == 4 &&
            profiled.ToT8(0x1234, 0, id, p) == C::Rules::Applied::Mapped && p[0] == 0, "setting() in a forward transform ignored the profile");
        Check(profiled.ToT7(0x1234, {2}, id, v, story) == C::Rules::Applied::Mapped && v == 3 &&
            profiled.ToT7(0x1234, {2}, id, v) == C::Rules::Applied::Mapped && v == 2, "setting() in a reverse transform ignored the profile");
    }
    {   // A computed id that ends a list drops the row instead of writing it.
        auto ids = C::Parse(base + "t8_to_t7_prop:0x1234 -> 0x5678  id = p0 ; v = p1\n");
        Check(ids.ToT7(0x1234, {7, 9}, id, v) == C::Rules::Applied::Mapped && id == 7 && v == 9, "id = EXPR");
        Check(ids.ToT7(0x1234, {0}, id, v) == C::Rules::Applied::Drop && ids.ToT7(0x1234, {881}, id, v) == C::Rules::Applied::Drop &&
            ids.ToT7(0x1234, {1100}, id, v) == C::Rules::Applied::Drop, "A computed list end was written");
    }
    // A newer valid file with one changed rule parses and carries the change.
    const auto current = "val:t7_t8_rules_version = " + std::to_string(rules.version);
    auto newer = C::Parse(replaced(current, "val:t7_t8_rules_version = " + std::to_string(rules.version + 1)) +
        "map:t7_t8_prop = 0x1234 -> none\n");
    Check(newer.version == rules.version + 1 && newer.Map("t7_t8_prop").Has(C::ToT8, 0x1234) && !newer.Map("t7_t8_prop").Map(C::ToT8, 0x1234), "Rules override line");
}

// Import profile (docs/import-profile.md): stored in the custom property list without touching the moveset data,
// defaults equal to the rules' defaults, carried and applied by the conversions and the retail import.
void RunImportProfileTests() {
    namespace IP = ImportProfile;
    using P = IP::Profile;
    Check(IP::FromProperties(IP::ToProperties({})) == P{}, "Default profile round trip");
    Check(IP::ConversionSettings({}) == ConversionRules::DefaultSettings(), "Profile defaults differ from the rules' setting() defaults");
    P custom; custom.identity = P::Identity::Fixed; custom.character = 9; custom.gender = P::Gender::Female;
    custom.storyFlag = 4; custom.inHeat = true; custom.flagsClear = false;
    Check(IP::FromProperties(IP::ToProperties(custom)) == custom, "Custom profile round trip");
    // Context options: the defaults decide nothing anywhere (behaviour before the options), each option its ids.
    for (uint32_t id = 0; id < 0x10000; ++id)
        for (auto game : {IP::Game::T7, IP::Game::T8})
            if (IP::RequirementOverride({}, game, id, {1, 2, 3, 4}) || IP::StripsProperty({}, game, id))
                throw std::runtime_error("Default import profile decides id " + std::to_string(id));
    P versus; versus.storyMode = versus.dlcStory = versus.specialModes = versus.stage = P::Toggle::Versus;
    versus.waveIndex = 0; versus.battleScript = 0; versus.item = P::Item::None; versus.bodyType = P::Body::Big;
    versus.storyEffects = versus.globalWrites = P::Strip::Strip;
    Check(IP::FromProperties(IP::ToProperties(versus)) == versus, "Context profile round trip");
    auto req8 = [&](uint32_t id, uint32_t x) { return IP::RequirementOverride(versus, IP::Game::T8, id, {x, 0, 0, 0}); };
    auto req7 = [&](uint32_t id) { return IP::RequirementOverride(versus, IP::Game::T7, id, {1, 0, 0, 0}); };
    Check(req8(667, 1) == false && req8(778, 0) == true && req8(796, 5) == true && req8(484, 0) == true && req8(484, 2) == false &&
        req8(485, 7) == true && req8(801, 0) == false && req8(803, 3) == true && req8(785, 0) == true && req8(785, 2) == false &&
        req8(771, 0) == true && req8(771, 3) == false && req8(664, 1) == false && req8(718, 9) == false && req8(691, 0) == true &&
        req8(687, 0) == false && req8(682, 4) == false && req8(683, 4) == true && !req8(35, 1),
        "T8 context requirement overrides");
    Check(req7(558) == false && req7(636) == false && req7(556) == false && req7(609) == false && req7(573) == false && req7(574) == true && !req7(35),
        "T7 context requirement overrides");
    Check(IP::StripsProperty(versus, IP::Game::T8, 34154) && IP::StripsProperty(versus, IP::Game::T8, 34159) &&
        IP::StripsProperty(versus, IP::Game::T7, 33439) && !IP::StripsProperty(versus, IP::Game::T7, 0x8001), "Property stripping");

    // A file gets a profile without its blocks or payload changing; a second write replaces it, other properties stay.
    auto source = SemanticBodyFixture();
    const auto& header = *reinterpret_cast<const TKMovesetHeader*>(source.data());
    Check(IP::ReadProperties(source).empty() && IP::Read(source) == P{}, "A file without a profile reads as the defaults");
    auto tail = [](std::span<const Byte> file) {
        const auto& h = *reinterpret_cast<const TKMovesetHeader*>(file.data());
        return std::vector<Byte>(file.begin() + h.block_list, file.end());
    };
    auto withOther = IP::WithProfile(source, {});
    {   // add an unrelated property by hand: it must survive profile edits
        auto properties = IP::ReadProperties(withOther); properties.insert(properties.begin(), TKMovesetProperty{0x1234, 77});
        properties.push_back({TKMovesetProperty_END, 0});
        const auto& h = *reinterpret_cast<const TKMovesetHeader*>(withOther.data());
        const auto listAt = (h.header_size + 7) & ~7u;
        std::vector<Byte> rebuilt(withOther.begin(), withOther.begin() + listAt);
        const auto listBytes = (properties.size() * sizeof(TKMovesetProperty) + 7) & ~size_t(7);
        rebuilt.resize(listAt + listBytes);
        memcpy(rebuilt.data() + listAt, properties.data(), properties.size() * sizeof(TKMovesetProperty));
        rebuilt.insert(rebuilt.end(), withOther.begin() + h.block_list, withOther.end());
        auto& rh = *reinterpret_cast<TKMovesetHeader*>(rebuilt.data());
        rh.moveset_data_start = static_cast<uint32_t>(listAt + listBytes + (h.moveset_data_start - h.block_list));
        rh.block_list = static_cast<uint32_t>(listAt + listBytes);
        withOther = std::move(rebuilt);
    }
    const auto edited = IP::WithProfile(withOther, custom);
    Check(IP::Read(edited) == custom && tail(edited) == tail(source), "Profile edit changed the moveset data");
    const auto properties = IP::ReadProperties(edited);
    Check(std::count_if(properties.begin(), properties.end(), [](const auto& p) { return p.id == 0x1234 && p.value == 77; }) == 1,
        "Profile edit dropped another property");
    Check(IP::WithProfile(edited, custom) == edited, "Profile edit is not idempotent");
    const auto& editedHeader = *reinterpret_cast<const TKMovesetHeader*>(edited.data());
    const auto& blocks = *reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(edited.data() + editedHeader.block_list);
    const auto payload = std::span<const Byte>(edited).subspan(editedHeader.moveset_data_start);
    std::string error;
    Check(T7ImportUtils::ValidateT7Semantics(blocks, payload.data(), payload.size(), &error), "Profiled file no longer validates: " + error);
    (void)header;

    // Settings reach the conditions: story flag 4 opens Jin's Devil chapter rows, Heat and flags follow the profile.
    const auto& rules = ConversionRules::Current();
    P devil; devil.storyFlag = 4; devil.inHeat = true; devil.flagsClear = false;
    const auto settings = IP::ConversionSettings(devil);
    Check(rules.RequirementHolds(ConversionRules::ToT7, 777, 0, {4}, settings) == true &&
        rules.RequirementHolds(ConversionRules::ToT7, 777, 0, {0}, settings) == false &&
        rules.RequirementHolds(ConversionRules::ToT7, 1028, 0, {1}, settings) == true &&
        rules.RequirementHolds(ConversionRules::ToT7, 476, 0, {16, 0}, settings) == false, "Profile settings in conditions");

    // Conversions carry the profile, with a fixed character in the target game's numbering.
    const auto& sh = *reinterpret_cast<const TKMovesetHeader*>(source.data());
    const auto& sb = *reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(source.data() + sh.block_list);
    P kazuya; kazuya.identity = P::Identity::Fixed; kazuya.character = 9; kazuya.gender = P::Gender::Male; // T7 Kazuya
    const auto t8 = T7ToRetailT8::ConvertStandalone(sh, sb, std::span<const Byte>(source).subspan(sh.moveset_data_start), nullptr, false, kazuya);
    const auto t8Profile = IP::Read(t8);
    Check(t8Profile.identity == P::Identity::Fixed && t8Profile.character == 8 && t8Profile.gender == P::Gender::Male,
        "T7 -> T8 conversion did not carry the profile (Kazuya: T7 9 -> T8 8)");
    const auto& th = *reinterpret_cast<const TKMovesetHeader*>(t8.data());
    const auto t7 = T8ToT7::Convert(th, std::span<const Byte>(t8).subspan(th.moveset_data_start), nullptr, false, t8Profile);
    const auto t7Profile = IP::Read(t7);
    Check(t7Profile.identity == P::Identity::Fixed && t7Profile.character == 9 && t7Profile.gender == P::Gender::Male,
        "T8 -> T7 conversion did not carry the profile (Kazuya: T8 8 -> T7 9)");

    // Retail import: gender and story flag rows become always / never true; identity picks the fixed character.
    std::vector<Byte> rows(5 * 20);
    const uint32_t ids[][2] = {{689, 0}, {690, 0}, {777, 4}, {777, 0}, {35, 3}};
    for (size_t i = 0; i < 5; ++i) { memcpy(rows.data() + i * 20, &ids[i][0], 4); memcpy(rows.data() + i * 20 + 4, &ids[i][1], 4); }
    const auto original = rows;
    Check(RetailT8Import::ApplyProfileConditions(rows, {}) == 0 && rows == original, "Default profile rewrote retail requirements");
    P female; female.gender = P::Gender::Female; female.storyFlag = 4;
    Check(RetailT8Import::ApplyProfileConditions(rows, female) == 4, "Retail profile rewrite count");
    auto id = [&](size_t i) { uint32_t v; memcpy(&v, rows.data() + i * 20, 4); return v; };
    const auto never = rules.Value("t7_to_t8_req_never_true");
    Check(id(0) == never && id(1) == 0 && id(2) == 0 && id(3) == never && id(4) == 35, "Retail gender / story flag rewrite");
    Check(RetailT8Import::ProfileIdentity(kazuya, 6) == 9 && RetailT8Import::ProfileIdentity({}, 6) == 6, "Retail profile identity");
}

void RunT8ToT7Tests() {
    RunPackedTrackTests();
    RunConversionRulesTests();
    RunImportProfileTests();
    Check(CanConvertMoveset(GameId_T8, GameId_T7, 0), "Retail T8 files are not offered for T7 conversion");
    Check(T8ToT7::CharacterT8ToT7(8) == 9 && T8ToT7::CharacterT8ToT7(6) == 6 && T8ToT7::CharacterT8ToT7(124) == UINT32_MAX,
        "T8 -> T7 character identities");
    {   // A static PANM stores any frame count in a few bytes: out-of-range positions are refused from the samples,
        // before ToChannels allocates and walks every frame (here a million).
        RetailPanm::Animation panm; panm.frames = 1000000;
        for (const char* name : {"Top", "Trans"}) {
            RetailPanm::Track track; track.name = name; track.samples = {RetailPanm::Sample{}};
            panm.tracks.push_back(track);
        }
        panm.tracks[0].samples[0].translation[0] = 3e38f;
        Reject([&] { RetailT8AnimToT7::ToChannels(panm); }, "Out-of-range PANM position accepted");
    }

    // Round trip: T7 -> retail T8 (self-contained) -> T7. Records, frames and references survive; the result is a
    // valid T7 file that converts the same way every time.
    namespace S = StructsT7;
    namespace G = StructsT7_gameAddr;
    auto source = SemanticBodyFixture();
    const auto& header = *reinterpret_cast<const TKMovesetHeader*>(source.data());
    const auto& blocks = *reinterpret_cast<const S::TKMovesetHeaderBlocks*>(source.data() + header.block_list);
    const auto t7 = std::span<const Byte>(source).subspan(header.moveset_data_start);
    const auto& info = *reinterpret_cast<const G::MovesetInfo*>(t7.data() + blocks.movesetInfoBlock);
    const auto t8 = T7ToRetailT8::ConvertStandalone(header, blocks, t7);
    const auto t8Copy = t8;
    {
        const auto t8Names = RetailT8Snapshot::Names(t8);
        Check(!t8Names.empty(), "T7 -> T8 conversion wrote no names section");
        for (const auto& [hash, name] : t8Names) Check(RetailNames::Hash(name) == hash, "Names section entry does not match its hash");
    }
    const auto& t8Header = *reinterpret_cast<const TKMovesetHeader*>(t8.data());
    const auto t8Payload = std::span<const Byte>(t8).subspan(t8Header.moveset_data_start);
    T8ToT7::Report report;
    const auto back = T8ToT7::Convert(t8Header, t8Payload, &report);
    Check(t8 == t8Copy, "T7 conversion modified its T8 input");
    Check(back == T8ToT7::Convert(t8Header, t8Payload), "T7 conversion is not deterministic");
    const auto& out = *reinterpret_cast<const TKMovesetHeader*>(back.data());
    Check(out.ValidateHeader(back.size()) && out.gameId == GameId_T7 && out.conversion_origin == GameVersions::T7::FROM_T8,
        "Converted T7 header");
    // The forward conversion's placeholder identity has no T7 counterpart.
    Check(out.characterId == ConversionRules::Current().Value("t8_to_t7_placeholder_character") && report.placeholderCharacter,
        "Converted T7 character identity");
    const auto& outBlocks = *reinterpret_cast<const S::TKMovesetHeaderBlocks*>(back.data() + out.block_list);
    const auto payload = std::span<const Byte>(back).subspan(out.moveset_data_start);
    std::string error;
    Check(T7ImportUtils::ValidateT7Semantics(outBlocks, payload.data(), payload.size(), &error), "Converted T7 file is invalid: " + error);
    const auto& outInfo = *reinterpret_cast<const G::MovesetInfo*>(payload.data());
    Check(outInfo.isInitialized && !memcmp(outInfo.orig_aliases, info.orig_aliases, sizeof(info.orig_aliases)) &&
        !memcmp(outInfo.current_aliases, info.current_aliases, sizeof(info.current_aliases)), "Aliases did not survive the round trip");
    // Retail holds 32 unknown aliases; T7 slots 32..35 must not take the identity words after them (retail 0x160..0x167).
    Check(!memcmp(outInfo.unknown_aliases, info.unknown_aliases, 32 * sizeof(uint16_t)) && outInfo.unknown_aliases[32] == 0 &&
        outInfo.unknown_aliases[33] == 0 && outInfo.unknown_aliases[34] == 0 && outInfo.unknown_aliases[35] == 0,
        "Unknown aliases took the retail identity words");
    for (size_t table : {size_t(0), size_t(2), size_t(4), size_t(5), size_t(6), size_t(8), size_t(12), size_t(14), size_t(15)})
        Check(outInfo.table.entries[table].listCount == info.table.entries[table].listCount,
            "Round trip changed the row count of table " + std::to_string(table));
    // Requirements: the forward guard row plus the never-true list.
    Check(outInfo.table.entries[1].listCount == info.table.entries[1].listCount + 3, "Requirement table rows");
    auto rows = [](std::span<const Byte> p, const S::TKMovesetHeaderBlocks& b, const G::MovesetInfo& i, size_t table) {
        return p.data() + b.movesetBlock + i.table.entries[table].listAddr;
    };
    const auto* inMoves = reinterpret_cast<const G::Move*>(rows(t7, blocks, info, 12));
    const auto* outMoves = reinterpret_cast<const G::Move*>(rows(payload, outBlocks, outInfo, 12));
    for (uint64_t row = 0; row < info.table.entries[12].listCount; ++row) {
        const auto& a = inMoves[row]; const auto& b = outMoves[row];
        const auto what = "Move " + std::to_string(row) + " ";
        Check(a.vuln == b.vuln && a.hitlevel == b.hitlevel, what + "vuln/hit level");
        Check(a.anim_len == b.anim_len && a.airborne_start == b.airborne_start && a.airborne_end == b.airborne_end &&
            a.ground_fall == b.ground_fall, what + "frames");
        Check(a.first_active_frame == b.first_active_frame && a.last_active_frame == b.last_active_frame &&
            a.distance == b.distance && a._0x98_int == b._0x98_int && a._0xA8_short == b._0xA8_short, what + "active frames");
        Check(a.transition == b.transition && a.cancel_addr == b.cancel_addr && a.hit_condition_addr == b.hit_condition_addr,
            what + "references");
        const auto* name = reinterpret_cast<const char*>(payload.data() + outBlocks.nameBlock + b.name_addr);
        const auto* original = reinterpret_cast<const char*>(t7.data() + blocks.nameBlock + a.name_addr);
        // The T8 file's names section (T8NM) carries the T7 strings through the round trip.
        Check(std::string(name) == original, what + "name did not survive the round trip through the names section");
        const auto* anim = reinterpret_cast<const char*>(payload.data() + outBlocks.nameBlock + b.anim_name_addr);
        const auto* originalAnim = reinterpret_cast<const char*>(t7.data() + blocks.nameBlock + a.anim_name_addr);
        Check(std::string(anim) == originalAnim, what + "animation name did not survive the round trip");
    }
    const auto* inCancels = reinterpret_cast<const G::Cancel*>(rows(t7, blocks, info, 6));
    const auto* outCancels = reinterpret_cast<const G::Cancel*>(rows(payload, outBlocks, outInfo, 6));
    for (uint64_t row = 0; row < info.table.entries[6].listCount; ++row)
        Check(inCancels[row].command == outCancels[row].command && inCancels[row].detection_start == outCancels[row].detection_start &&
            inCancels[row].starting_frame == outCancels[row].starting_frame && inCancels[row].move_id == outCancels[row].move_id,
            "Cancel " + std::to_string(row) + " did not survive the round trip");
    Check(report.animationsConverted > 0 && report.animationsFallback == 0 && report.animationsConversionFailed == 0,
        "Embedded converted animations were not turned back into T7 animations");
    const auto json = T8ToT7::ReportJson(report);
    Check(json["moves"] == info.table.entries[12].listCount, "Conversion report move count");
    Check(json["properties"]["extra_requirements_dropped"] == 0, "Ungated extra properties counted as requirement-gated");

    // A retail special command with no T7 rule that lands on a T7 list command (here retail 0x800b, the T7 group cancel)
    // is disabled as command 0, not copied: its move id would be read as a group cancel row (T8 Clive Rosfield refusal).
    {
        auto patched = std::vector<Byte>(t8Payload.begin(), t8Payload.end());
        const auto snapshot = *reinterpret_cast<const RetailT8Snapshot::Header*>(patched.data());
        const auto cancelTable = *reinterpret_cast<const RetailT8Snapshot::Entry*>(
            patched.data() + snapshot.directoryOffset + 6 * sizeof(RetailT8Snapshot::Entry));
        uint64_t row = 0;
        auto at = [&](uint64_t r) { return patched.data() + cancelTable.offset + r * cancelTable.stride; };
        while (row < cancelTable.count && *reinterpret_cast<const uint64_t*>(at(row)) >= 0x8000) ++row;
        Check(row < cancelTable.count, "Fixture has no plain cancel to patch");
        const auto moveCount = static_cast<uint16_t>(info.table.entries[12].listCount);
        const auto groupCount = info.table.entries[7].listCount;
        const uint16_t target = static_cast<uint16_t>(moveCount - 1);
        *reinterpret_cast<uint64_t*>(at(row)) = 0x800b;
        *reinterpret_cast<uint16_t*>(at(row) + 36) = target;
        T8ToT7::Report patchedReport;
        const auto converted = T8ToT7::Convert(t8Header, patched, &patchedReport); // threw when target >= groupCount
        const auto& h = *reinterpret_cast<const TKMovesetHeader*>(converted.data());
        const auto& b = *reinterpret_cast<const S::TKMovesetHeaderBlocks*>(converted.data() + h.block_list);
        const auto p = std::span<const Byte>(converted).subspan(h.moveset_data_start);
        const auto& i = *reinterpret_cast<const G::MovesetInfo*>(p.data());
        const auto& cancel = reinterpret_cast<const G::Cancel*>(rows(p, b, i, 6))[row];
        const auto* requirement = reinterpret_cast<const S::Requirement*>(rows(p, b, i, 1)) + cancel.requirements_addr;
        Check(cancel.command == 0 && cancel.move_id == target && cancel.requirements_addr < i.table.entries[1].listCount &&
            requirement->condition == ConversionRules::Current().Value("t8_to_t7_req_never_true") && requirement[1].condition == 881,
            "Unmapped retail 0x800b cancel is not disabled as a never-true command 0 (group count " + std::to_string(groupCount) + ")");
        Check(patchedReport.specialCancelsDisabled == report.specialCancelsDisabled + 1, "Disabled special cancel count");
    }
    // A requirement row a t8_to_t7_prop transform drops stays never true, all-zero parameters included: retail 0x87 (throw
    // break button) has no T7 id for p0 = 0, and "unknown zero parameters hold" made it always true.
    {
        auto patched = std::vector<Byte>(t8Payload.begin(), t8Payload.end());
        const auto snapshot = *reinterpret_cast<const RetailT8Snapshot::Header*>(patched.data());
        const auto requirementTable = *reinterpret_cast<const RetailT8Snapshot::Entry*>(
            patched.data() + snapshot.directoryOffset + 1 * sizeof(RetailT8Snapshot::Entry));
        auto at = [&](uint64_t r) { return patched.data() + requirementTable.offset + r * requirementTable.stride; };
        uint64_t row = 0;
        while (row < requirementTable.count && *reinterpret_cast<const uint32_t*>(at(row)) == 1100) ++row;
        Check(row < requirementTable.count && requirementTable.stride >= 20, "Fixture has no requirement row to patch");
        memset(at(row), 0, 20); *reinterpret_cast<uint32_t*>(at(row)) = 0x87;
        const auto converted = T8ToT7::Convert(t8Header, patched);
        const auto& h = *reinterpret_cast<const TKMovesetHeader*>(converted.data());
        const auto& b = *reinterpret_cast<const S::TKMovesetHeaderBlocks*>(converted.data() + h.block_list);
        const auto p = std::span<const Byte>(converted).subspan(h.moveset_data_start);
        const auto& i = *reinterpret_cast<const G::MovesetInfo*>(p.data());
        const auto* requirement = reinterpret_cast<const S::Requirement*>(rows(p, b, i, 1)) + row;
        Check(requirement->condition == ConversionRules::Current().Value("t8_to_t7_req_never_true"),
            "A requirement row dropped by its transform became always true");
    }

    // Refusals: anything but a retail T8 file, and a damaged snapshot.
    auto cnt = t8Header; cnt.minorVersion = GameVersions::T8::CLOSED_NETWORK_TEST;
    Reject([&] { T8ToT7::Convert(cnt, t8Payload); }, "A CNT file was converted with the retail layout");
    Reject([&] { T8ToT7::Convert(header, t7); }, "A T7 file was converted as T8");
    auto damaged = std::vector<Byte>(t8Payload.begin(), t8Payload.end()); damaged[0] = 'X';
    Reject([&] { T8ToT7::Convert(t8Header, damaged); }, "A damaged retail snapshot was converted");
}
