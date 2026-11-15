#pragma once
#include <cstdint>
#include <format>
#include <string>

// Decoders for retail Tekken 8 move fields shown as combos next to the raw value.
// Evidence (2026-11-14, Tekken8 cnt project, all 52 retail motbins, work/tkdata_retail/readable/*/moves.csv):
// - hit_level: the low 12 bits are the attack type. 0x512 high, 0x217 mid, 0x10F low and 0x41F (treated as
//   low by the "incoming low" requirement handlers 70/781, blocked standing too: special mid) are confirmed by
//   the retail requirement handlers (docs/retail-moveset/requirements_retail.csv ids 68-71, 779-781). 0xA00 is
//   the throw type (3,530 moves, all *_n throw reactions and throw moves).
//   0x707 (Nina/Anna kiss, Naginata), 0x31F (item knives), 0xB07 (item guns) and 0x806 (beams) are named from
//   their moves only. Bits 16-23 are flags (0x20 on most character attacks, 0x40 on throw moves, 0x80 on
//   special/story moves, 0x2C/0x3C on Heat and rage moves); they are kept untouched by the combo.
// - hitbox location: 0x000000aa = one limb aa (line from its previous-frame to current position),
//   0x0000aabb = a line between limbs aa and bb. Limb ids by move-name correlation:
//   0x9 right hand (1,404 of the *rp* moves), 0xD left hand (1,744 *lp*), 0x10 right foot (1,844 *rk*),
//   0x13 left foot (1,905 *lk*); the neighbours 0x8/0xC/0xF/0x12 and the lines 0x809/0xC0D/0xF10/0x1213
//   follow the same punch/kick split and are read as forearm/shin (hypothesis).
namespace RetailFieldCodecs {
struct Choice { uint32_t value; const char* label; bool confirmed; };

inline constexpr uint32_t HitLevelMask = 0xFFF;
inline constexpr Choice HitLevels[] = {
    {0x000, "None", true},
    {0x512, "High", true},
    {0x217, "Mid", true},
    {0x10F, "Low", true},
    {0x41F, "Special mid", true},
    {0xA00, "Throw", false},
    {0x707, "Unblockable", false},
    {0x31F, "Item: knife", false},
    {0xB07, "Item: gun", false},
    {0x806, "Beam", false},
};
inline const Choice* HitLevelChoice(uint32_t value) {
    for (const auto& choice : HitLevels) if (choice.value == (value & HitLevelMask)) return &choice;
    return nullptr;
}
inline std::string HitLevelText(uint32_t value) {
    const auto* choice = HitLevelChoice(value);
    auto text = choice ? std::format("{} (0x{:X})", choice->label, value & HitLevelMask) : std::format("0x{:X} (unknown)", value & HitLevelMask);
    if (value & ~HitLevelMask) text += std::format(" + flags 0x{:X}", value >> 12);
    return text;
}

inline constexpr Choice Limbs[] = {
    {0x9, "Right hand", true},
    {0xD, "Left hand", true},
    {0x10, "Right foot", true},
    {0x13, "Left foot", true},
    {0x8, "Right forearm", false},
    {0xC, "Left forearm", false},
    {0xF, "Right shin", false},
    {0x12, "Left shin", false},
};
inline std::string LimbName(uint32_t limb) {
    for (const auto& choice : Limbs) if (choice.value == limb) return choice.confirmed ? choice.label : std::format("{}?", choice.label);
    return std::format("limb 0x{:X}", limb);
}
// Combo entries for a hitbox location: single limbs and the four forearm/shin lines.
inline constexpr Choice HitboxLocations[] = {
    {0x0, "None", true},
    {0x9, "Right hand", true},
    {0xD, "Left hand", true},
    {0x10, "Right foot", true},
    {0x13, "Left foot", true},
    {0x809, "Right forearm -> right hand", false},
    {0xC0D, "Left forearm -> left hand", false},
    {0xF10, "Right shin -> right foot", false},
    {0x1213, "Left shin -> left foot", false},
};
// Cancel option (+0x26): 0 on disk, filled by the game at load. Live values of 34,463 cancels (3 retail movesets,
// Tekken8 cnt work/movevm/port/retail_live_movesets.json): 0x1 <=> command is 0 (10,931/10,931, never otherwise);
// 0x10 <=> command is not 0 (23,514/23,532); 0x100 <=> the target is a generic alias >= 0x8000 (8,889/8,889);
// 0x80 follows command flag 0x20000000 in the high dword (2,128/2,228 of those commands, 114 others); 0x40 is set on
// 89% of cancels and on every cancel whose starting frame is 0 (its rule is open); 0x2/0x4/0x8/0x200 are rare
// (< 330 each) and only occur with a nonzero window start, starting frame and requirements.
struct CancelOptionBit { uint16_t bit; const char* label; bool confirmed; };
inline constexpr CancelOptionBit CancelOptionBits[] = {
    {0x1, "no command", true},
    {0x10, "has command", true},
    {0x100, "alias target", true},
    {0x80, "command flag 0x20000000", false},
    {0x40, "bit 0x40", false},
    {0x2, "bit 0x2", false},
    {0x4, "bit 0x4", false},
    {0x8, "bit 0x8", false},
    {0x200, "bit 0x200", false},
};
inline std::string CancelOptionText(uint32_t value) {
    if (!value) return "0 (not filled: set by the game at load)";
    std::string text; uint32_t known = 0;
    for (const auto& item : CancelOptionBits) if (value & item.bit) {
        if (!text.empty()) text += " | ";
        text += item.label; if (!item.confirmed) text += "?";
        known |= item.bit;
    }
    if (value & ~known) { if (!text.empty()) text += " | "; text += std::format("0x{:X}", value & ~known); }
    return text;
}
// Vulnerability (move +0x58, copied to the player's posture word +0x434 at move start; req 0x3D compares it whole).
// Handler evidence (Tekken8 cnt docs/move-language-retail.md, requirements 63/64/66/67/128): bits 0-2 are the posture
// (2 standing, 1 crouching, 4 downed), 0x400 is airborne. Data, 52 retail motbins (102 values): 0x842 on 78,100 moves
// (standing), 0xC842 jumps (standing bits + 0x4000|0x8000, no 0x400), 0x7402 knocked-down air moves (0x400 set), 0x6084 downed (dwDWN), 0x2821
// crouching, 0x3029 crouch wake-ups (cOKI), 0x1052 standing wake-ups (sOKI), 0xA000 no posture (balcony falls,
// character change). The other low bits (0x8, 0x10, 0x20, 0x40, 0x80, 0x800, 0x1000-0x8000) and the high 16 bits
// (0x8, 0x100, 0x1, 0x2 ...) are not decoded; the combo keeps them.
inline constexpr uint32_t VulnerabilityPostureMask = 0x407;
inline constexpr Choice Vulnerabilities[] = {
    {0x842, "Standing (default)", true},
    {0xC842, "Standing (jump)", true},
    {0x7402, "Standing, airborne (knocked down)", true},
    {0x1052, "Standing (wake-up)", true},
    {0x2821, "Crouching", true},
    {0x3029, "Crouching (wake-up)", true},
    {0x6084, "Downed", true},
    {0x6A84, "Downed (moving)", true},
    {0xA000, "No posture", true},
};
inline std::string VulnerabilityText(uint32_t value) {
    std::string text;
    switch (value & 7) {
    case 2: text = "Standing"; break;
    case 1: text = "Crouching"; break;
    case 4: text = "Downed"; break;
    case 0: text = "No posture"; break;
    default: text = std::format("posture 0x{:X}", value & 7); break;
    }
    if (value & 0x400) text += " + airborne";
    const auto other = value & 0xFFFF & ~VulnerabilityPostureMask;
    if (other) text += std::format(" + 0x{:X}", other);
    if (value >> 16) text += std::format(" + flags 0x{:X}", value >> 16);
    return text;
}
inline std::string HitboxLocationText(uint32_t value) {
    if (!value) return "None";
    std::string text;
    if (value < 0x100) text = LimbName(value);
    else text = LimbName((value >> 8) & 0xFF) + " -> " + LimbName(value & 0xFF);
    if (value > 0xFFFF) text += std::format(" (+0x{:X})", value >> 16);
    return text;
}

// Meaning of a requirement's first parameter, for the ids where the value is a code rather than a number; empty
// for every other id. Sources (Tekken8 cnt project, findings log 2026-11-14): the retail handler review
// (docs/retail-moveset/requirements_retail.csv: fighter id at +0x168 with aliases 0x81..0x8F, packed flag compares,
// button masks) and LukzB/TekkenMovesetExtractor additionalReqDetails.py (game modes, controller types, story
// battle code; names marked "?" are theirs too). characterName(id) names a fighter (runtime) id.
template <class CharacterName>
inline std::string RequirementParamText(uint32_t id, uint32_t param, CharacterName&& characterName) {
    const uint32_t low = param & 0xFFFF, high = param >> 16;
    switch (id) {
    case 220: case 221: case 222: case 223: { // fighter id at P/O+0x168, through FUN_14187AE80's aliases
        static constexpr uint32_t aliases[][2] = {{0x81, 0x17}, {0x82, 0x18}, {0x83, 0x21}, {0x86, 0x2A},
            {0x88, 0x05}, {0x8A, 0x2B}, {0x8C, 0x16}, {0x8E, 0x0E}};
        for (const auto& alias : aliases) if (low == alias[0]) return "alias: " + characterName(alias[1]);
        if (low >= 0x81 && low <= 0x8F) return "alias, never matches";
        return characterName(low);
    }
    case 228: case 229: // controller type
        switch (low) { case 0: return "player"; case 1: return "CPU"; case 3: return "intro/outro?"; default: return {}; }
    case 288: return std::format("flag {} >= {}", high, low);
    case 326: return std::format("flag {} <= {}", high, low);
    case 365: return std::format("flag {} == {}", high, low);
    case 453: { // buttons held, bits 0..6 in the cancel button order
        static constexpr const char* buttons[] = {"1", "2", "3", "4", "HE", "SS", "RA"};
        std::string text;
        for (int bit = 0; bit < 7; ++bit) if (param & (1u << bit)) text += (text.empty() ? "" : "+") + std::string(buttons[bit]);
        return text;
    }
    case 668: return std::format("chapter {}, battle {}", (param >> 4) & 0xF, param & 0xF); // story battle code
    case 672: // game mode (round +0x6C)
        switch (low) {
        case 0: return "Arcade"; case 1: return "Practice"; case 4: return "Story"; case 5: return "Character Episode";
        case 6: return "Customization"; case 10: return "Versus"; default: return {};
        }
    default: return {};
    }
}
}
