#pragma once
#include "RetailPanm.hpp"
#include <string>

// T7 wing animation (wing MOTA, slots 6/7) -> retail T8 swing PANM (anim-manager category 3).
// Each T7 bone is a channel group: position (0x0B), rotation (0x07), optional scale (0x03). The retail rotation is the
// inverse of the T7 0x64 axis-angle rotation (as for hands, not mirrored), positions are /10, scale is copied. The T8
// bone names come from the built-in rigs (T7WingToRetailT8Rigs.inc, the rigs of native T8 swing PANMs that T8 Kazuya's
// own swing list plays): the animation goes onto the rig with its bone count whose rest offsets its frame-0 positions
// match. Checked against native T8 swing PANMs (.docs/t7-to-retail-t8-face-wings.md).
namespace T7WingToRetailT8 {
// Largest difference (cm) between a bone's frame-0 position and its rig's rest offset for the rig to be chosen.
inline constexpr float RestTolerance = 0.05f;
// The T8 PANM (raw Transform tracks, no options, like native raw swing PANMs). `rig` receives the chosen rig's source.
// Throws when the channel layout is not bones of (position, rotation[, scale]) or no rig matches its rest offsets.
std::vector<Byte> Convert(std::span<const Byte> t7Animation, std::string* rig = nullptr);

// T7 "Play Wing Animations" (value: slot 6, or slot 7 with bit 0x4000) and its retail id (alias catalog, static),
// whose value is the category-3 index. Over 51 T7 characters only Kazuya plays wings this way (19 rows).
struct WingProperty { uint32_t t7, retail; };
inline constexpr WingProperty WingProperties[] = {{33840, 0x8683}};
inline const WingProperty* FindT7Property(uint32_t id) {
    for (const auto& p : WingProperties) if (p.t7 == id) return &p;
    return nullptr;
}
inline const WingProperty* FindRetailProperty(uint32_t id) {
    for (const auto& p : WingProperties) if (p.retail == id) return &p;
    return nullptr;
}
inline constexpr uint32_t T7CharacterMota = 0x4000;
inline constexpr uint32_t WingCategory = 3;
inline constexpr uint32_t FirstRetailIndex = 0x2000, LastRetailIndex = 0x3FFF;
inline constexpr uint32_t AssetInstance(uint32_t index) { return 0x00030000 | index; }
inline constexpr uint32_t AssetIndex(uint32_t instance) { return instance & 0xFFFF; }
}
