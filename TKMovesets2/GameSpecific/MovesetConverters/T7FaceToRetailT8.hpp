#pragma once
#include "RetailPanm.hpp"

// T7 face animation (face MOTA, slots 4/5) -> retail T8 face PANM (anim-manager category 2) on the legacy 43-bone
// `*_Joint` face rig, which T8 still ships (28 native T8 face PANMs use it; .docs/t7-to-retail-t8-face-wings.md).
// T7 channels 2k / 2k+1 are bone k's position (0x0B) and rotation (0x07), k = 0..42; the four trailing 0x03 channels
// are 1.0 in every T7 face clip and are dropped. Rotation = the inverse of the 0x64 axis-angle rotation, position / 10.
// Written as native face PANMs are: packed tracks (static when a bone never moves), options {3, 1}, {4, 1}.
// Playable T8 characters use a 185/186-bone face rig instead: on them these bones may not exist.
namespace T7FaceToRetailT8 {
inline constexpr size_t Bones = 43;
// Throws for any other channel layout (T7 Armor King's 70-channel and Negan's 116-channel faces).
std::vector<Byte> Convert(std::span<const Byte> t7Animation);

// T7 "Play Facial Animation Sequence" (value: slot 4, or slot 5 with bit 0x4000) and its retail id (alias catalog,
// static: "Play Facial Animation"), whose value is the category-2 index. 639 rows over 48 T7 characters.
struct FaceProperty { uint32_t t7, retail; };
inline constexpr FaceProperty FaceProperties[] = {{33986, 0x877A}};
inline const FaceProperty* FindT7Property(uint32_t id) {
    for (const auto& p : FaceProperties) if (p.t7 == id) return &p;
    return nullptr;
}
inline const FaceProperty* FindRetailProperty(uint32_t id) {
    for (const auto& p : FaceProperties) if (p.retail == id) return &p;
    return nullptr;
}
inline constexpr uint32_t T7CharacterMota = 0x4000;
inline constexpr uint32_t FaceCategory = 2;
inline constexpr uint32_t FirstRetailIndex = 0x2000, LastRetailIndex = 0x3FFF;
// Instances apart from hands (0x20000), wings (0x30000) and cameras (0x40000).
inline constexpr uint32_t AssetInstance(uint32_t index) { return 0x00050000 | index; }
inline constexpr uint32_t AssetIndex(uint32_t instance) { return instance & 0xFFFF; }
}
