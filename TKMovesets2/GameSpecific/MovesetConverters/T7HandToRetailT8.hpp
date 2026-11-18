#pragma once
#include "RetailPanm.hpp"
#include <optional>

// T7 hand animation (hand MOTA, slots 2/3) -> retail T8 hand PANM (category 1: 19 finger bones of one hand).
// Closed form, as PolarisAnimConverter's hand converter (.docs/polaris-anim-converter.md, "Hand animations"): each
// T7 channel is one finger joint, and the T8 rotation is the inverse of the T7 0x64 axis-angle rotation (mirrored for
// the left hand). The hand comes from the property that plays the animation: T8 keeps a left and a right copy of the
// same T7 animation.
namespace T7HandToRetailT8 {
enum class Side { Left, Right };
// T7 "Play Left/Right Hand Animation" and their retail ids. The T7 value indexes hand MOTA slot 2 (shared hands), or
// slot 3 (the character's own) when bit 0x4000 is set; all 51 T7 characters use only these two (13,776 rows, every
// index in range). The retail value is an index into the character's hand category (anim-manager category 1): retail
// masks bits 14 and 15 out of it, and native lists hold a few hundred entries.
struct HandProperty { uint32_t t7, retail; Side side; };
inline constexpr HandProperty HandProperties[] = {{0x842E, 0x8610, Side::Left}, {0x842F, 0x8611, Side::Right}};
inline const HandProperty* FindT7Property(uint32_t id) {
    for (const auto& p : HandProperties) if (p.t7 == id) return &p;
    return nullptr;
}
inline const HandProperty* FindRetailProperty(uint32_t id) {
    for (const auto& p : HandProperties) if (p.retail == id) return &p;
    return nullptr;
}
inline constexpr uint32_t T7CharacterMota = 0x4000;
// Retail hand indices converted animations use: above every native list, below the masked flag bits.
inline constexpr uint32_t FirstRetailIndex = 0x2000, LastRetailIndex = 0x3FFF;
inline constexpr uint32_t HandCategory = 1;
// T8AN instance of a converted hand animation: its retail index, apart from converted body instances (0x10000 + n).
inline constexpr uint32_t AssetInstance(uint32_t index) { return 0x00020000 | index; }
inline constexpr uint32_t AssetIndex(uint32_t instance) { return instance & 0xFFFF; }
// T7 hand animations have 19 channels (thumb 1..3, then index, middle, ring and pinky 0..3) or 15 (no metacarpals).
bool Convertible(const RetailPanm::T7Animation& source);
// The T8 hand animation for `side`: every T7 channel becomes its finger bone's rotation, translations keep the native
// rest offsets (T7HandToRetailT8Skeleton.inc); bones the T7 animation does not drive keep the native rest pose.
std::vector<Byte> Convert(const RetailPanm::T7Animation& source, Side side);
// The animations of one T7 MOTA archive (`mota`: from the archive's start to the end of the MOTA block), in list
// order. Each span runs from the animation to the end of `mota` (the T7 decoder reads its own extent). Throws for a
// malformed archive.
std::vector<std::span<const Byte>> MotaAnimations(std::span<const Byte> mota);
}
