#pragma once
#include "RetailPanm.hpp"
#include <optional>

// T7 camera animation (camera MOTA, slots 8/9) -> retail T8 camera PANM (anim-manager category 4).
// Recipe from PolarisAnimConverter's camera converter, checked on every T7 camera against every native T8 camera of
// the same length (.docs/polaris-anim-converter.md, "Camera animations"): positions /10, ZYX Euler or look-at
// rotation, roll, FOV = zoom x 9/16, smoothing copied into "Camera_Param". The PANM has the native camera layout
// (RetailPanm::EncodeGroups).
namespace T7CameraToRetailT8 {
// T7 camera channel descriptors.
inline constexpr uint32_t Position = 0x0B, Rotation = 0x07, Zoom = 0x0F, Smoothing = 0x03, Roll = 0x15;
// A decoded T7 0xC8 camera clip: descriptor per channel, three floats per channel per frame.
struct Camera { uint32_t frames = 0; std::vector<uint32_t> descriptors; std::vector<float> values; };
// Reads a 0xC8 clip (either endianness). T7 plays no 0x64 camera clip; those are refused.
Camera Read(std::span<const Byte> animation, uint32_t maxFrames = 100000);
// The layouts T7 uses: camera position + Euler rotation (+ roll, + up to two extra roll-type channels), or target +
// camera position (look-at) + smoothing + roll; every one has a zoom channel.
bool Convertible(const Camera& camera);
std::vector<Byte> Convert(const Camera& camera);

// T7 properties that play a camera animation by value (bit 0x4000: the character's MOTA, slot 9; else slot 8) and
// their retail ids (alias catalog, status static), whose value is the category-4 index. Over 51 T7 characters:
// 3,694 rows, all in extra-property lists, every index in range.
// T7 33845 plays the camera whatever the side. Its catalog target 0x8688 ignores its value in retail (native data
// only uses 0): in game a converted 0x8688 row flew the camera across the stage and left it there (Kazuya, 2026-11-18).
// It becomes the side pair 0x868C (side 0) + 0x868D (side 1), which play the value as the camera index on the
// player, like T7 33849/33850; checked in game the same day (camera on the fighter, then back to the battle camera).
struct CameraProperty { uint32_t t7, retail, paired = 0; };
inline constexpr CameraProperty CameraProperties[] = {
    {33849, 0x868C}, {33850, 0x868D}, {33845, 0x868C, 0x868D}, {33861, 0x8698}, {33862, 0x8699}, {33863, 0x869A}, {33864, 0x869B}, {33865, 0x869C}};
inline const CameraProperty* FindT7Property(uint32_t id) {
    for (const auto& p : CameraProperties) if (p.t7 == id) return &p;
    return nullptr;
}
inline const CameraProperty* FindRetailProperty(uint32_t id) {
    for (const auto& p : CameraProperties) if (p.retail == id) return &p;
    return nullptr;
}
inline constexpr uint32_t T7CharacterMota = 0x4000;
inline constexpr uint32_t CameraCategory = 4;
// Retail camera indices converted animations use (as for hands: above native lists, below the masked flag bits).
inline constexpr uint32_t FirstRetailIndex = 0x2000, LastRetailIndex = 0x3FFF;
inline constexpr uint32_t AssetInstance(uint32_t index) { return 0x00040000 | index; }
inline constexpr uint32_t AssetIndex(uint32_t instance) { return instance & 0xFFFF; }
}
