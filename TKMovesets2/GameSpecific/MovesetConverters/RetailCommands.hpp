#pragma once
#include <cstdint>
#include <format>
#include <string>

// Retail Tekken 8 cancel/input command values. Retail renumbered T7's special commands
// (see T7ToRetailT8Standalone.cpp, Command()): group cancel 0x800b -> 0x8012, group end
// 0x800c -> 0x8013, input sequence base 0x800d -> 0x8014.
// Checked on the 52 retail motbins (2026-11-14): 34,337 group cancels, all pointing at a
// group-list start; 4,403 group lists, all ending with 0x8013 (0x800c never occurs); with
// base 0x8014 every input-sequence command is inside input_sequences (base 0x800d: 329
// out of range). 0x800d and 0x8010 are ordinary retail specials, not input sequences.
namespace RetailT8Commands {
inline constexpr uint64_t ListEnd = 0x8000;           // cancel list terminator ("AUTO" in the T7 editor)
inline constexpr uint64_t GroupCancel = 0x8012;       // move_id = group_cancels row
inline constexpr uint64_t GroupEnd = 0x8013;          // group_cancels list terminator
inline constexpr uint32_t InputSequenceBase = 0x8014; // command - base = input_sequences row

inline bool IsInputSequence(uint64_t command) {
    const auto low = static_cast<uint32_t>(command);
    return low >= InputSequenceBase && low < 0x10000;
}
inline uint32_t InputSequenceId(uint64_t command) { return static_cast<uint32_t>(command) - InputSequenceBase; }

// Direction bits 1..9 in the low dword as in T7. High dword: buttons 1-4 in bits 0..3, Heat in bit 4,
// Special Style in bit 5 (label from LukzB's editor; no retail cancel uses it), Rage Art in bit 6, and
// bit 29 = any one of the listed buttons (shown with "|"). Checked on the 52 retail motbins
// (2026-11-14): bit-4 cancels lead to *_HeatSmash / *_EX moves, bit-6 cancels to *_RageArts00 and
// Co_RageBurst, bit-29 cancels to the ukemi moves. T7's Rage Art bit 4 becomes bit 6 in the
// converter (T7ToRetailT8Standalone.cpp, Command()). Port of Editor_t7_Commands.cpp getCommandString.
inline std::string CommandString(uint64_t command) {
    static constexpr const char* directions[] = {"D/B", "D", "D/F", "B", "N", "F", "U/B", "U", "U/F"};
    if (command == ListEnd) return "AUTO";
    if (command == GroupCancel) return "GROUP";
    if (command == GroupEnd) return "END";
    if (IsInputSequence(command)) return std::string();
    // Other retail specials (0x8001..0x8011): no direction meaning, show the raw value.
    if ((command >> 32) == 0 && command > ListEnd) return std::format("0x{:X}", command);
    const auto directionBits = static_cast<uint32_t>(command);
    const auto inputBits = static_cast<uint32_t>(command >> 32);
    std::string result;
    for (int i = 1; i < 10; ++i)
        if (directionBits & (1u << i)) { if (!result.empty()) result += "|"; result += directions[i - 1]; }
    if ((directionBits >> 10) || (directionBits & 1)) { if (!result.empty()) result += "|"; result += "???"; }
    std::string inputs;
    for (int i = 0; i < 4; ++i) if (inputBits & (1u << i)) inputs += "+" + std::to_string(i + 1);
    if (inputBits & (1u << 4)) inputs += "+HE";
    if (inputBits & (1u << 5)) inputs += "+SS";
    if (inputBits & (1u << 6)) inputs += "+RA";
    if (inputBits & (1u << 29)) for (auto& c : inputs) if (c == '+' && &c != &inputs[0]) c = '|';
    if (!inputs.empty()) result += inputs.substr(result.empty() ? 1 : 0);
    return result;
}
inline std::string CommandString(uint32_t direction, uint32_t buttons) {
    return CommandString((uint64_t(buttons) << 32) | direction);
}
}
