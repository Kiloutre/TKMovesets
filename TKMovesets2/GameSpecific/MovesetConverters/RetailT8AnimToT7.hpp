#pragma once
#include "RetailPanm.hpp"

// Retail T8 PANM body animation -> T7 0xC8 (per-frame floats) or 0x64 (keyframed bitstream).
// Inverse of T7AnimToRetailT8's closed-form retargeting. The output uses the T8->T7 convention that
// T7AnimToRetailT8 already recognizes: LowerBody pinned at its rest pose, pelvis orientation carried by
// FullBody. Bones the forward conversion ignores (Hip, HARA_ROT1, fingers, ...) are dropped, and only the
// Z twist of the lower arms and lower legs is kept, as in the forward direction.
namespace RetailT8AnimToT7 {
enum class Format : uint16_t { Keyframed = 0x64, Frames = 0xC8 };
// The 23 standard T7 body channel types (11 = position, others rotation; 0x64 stores 4..7 as int16).
inline constexpr uint16_t ChannelTypes[23] = {11,11,5,7,7,7,11,7,7,7,7,6,7,7,7,6,7,7,6,7,7,6,7};
// Raw/static PANM -> the 23 T7 body channels in 0xC8 form (positions, ZYX Euler radians; keyframed = false).
// Requires Top and Trans; other missing bones keep their rest pose.
RetailPanm::T7Animation ToChannels(const RetailPanm::Animation& panm);
// Little-endian writers for 23-channel 0xC8-form input (keyframed = false).
std::vector<Byte> WriteC8(const RetailPanm::T7Animation& channels);
// 0x64: rotations become KEF triples quantized to pi/32769 steps, positions use one float scale per channel
// group; 16-frame keyframe blocks with the same variable-length codes ParseAnimation0x64 reads. At most 65535 frames.
std::vector<Byte> Write64(const RetailPanm::T7Animation& channels);
// PANM bytes (raw/static tracks) -> T7 animation bytes.
std::vector<Byte> Convert(std::span<const Byte> panm, Format format);
}
