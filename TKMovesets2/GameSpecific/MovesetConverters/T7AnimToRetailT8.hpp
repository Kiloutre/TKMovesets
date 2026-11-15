#pragma once
#include "RetailPanm.hpp"
#include <optional>

// T7 0x64/0xC8 body animation -> retail T8 PANM (raw Transform tracks).
// Closed-form retargeting: 0x64 KEF triples
// become ZYX Euler, then each T8 bone is a constant frame change of one T7 channel; T7 UpperBody is split
// over Spine1/Spine2. Against native T8 Kazuya (250 same-capture moves) every converted bone except the
// spine matches within 0.002 rad; Spine1/Spine2 within 0.05/0.11 rad.
namespace T7AnimToRetailT8 {
// Address used in T8AN asset entries for converted (non-native) animations.
// The importer compiles these in the game and attaches them to the carrier
// resource named by the entry's instance ID instead of comparing bytes.
inline constexpr uint64_t CustomAssetAddress = 0x00007FFFFFFF0000ULL;
// Channels (bones) a T7 animation needs for Convert: the 23 standard T7 body channels.
inline constexpr uint16_t BodyChannels = 23;
// True when a native PANM can serve as template: every track raw/static, Top and Trans, at least 20 bones.
bool UsableTemplate(std::span<const Byte> panm);
// Converts using the template's skeleton (bone set and order, track options, frame-0 pose of unmapped
// bones) and writes the result with RetailPanm::Encode (native layout, accepted by the game).
std::vector<Byte> Convert(const RetailPanm::T7Animation& source, std::span<const Byte> templatePanm);
// The built-in retail body skeleton (43 bones, native order and track options, frame-0 pose of a native idle).
const RetailPanm::Animation& BuiltInSkeleton();
// Converts onto the built-in skeleton: the result depends only on the T7 animation (no template, no donor).
std::vector<Byte> Convert(const RetailPanm::T7Animation& source);
// The built-in retail body skeleton (43 bones, native order and track options, frame-0 pose of a native idle).
const RetailPanm::Animation& BuiltInSkeleton();
// Converts onto the built-in skeleton: the result depends only on the T7 animation (no template, no donor).
std::vector<Byte> Convert(const RetailPanm::T7Animation& source);
// Most frames a T7 animation can have while its built-in-skeleton conversion still fits the 32 MiB PANM budget:
// every converted bone becomes a full-length track. Necessary, not sufficient (Encode still checks the exact
// size), so longer animations are refused before the costly decode without rejecting any that would convert.
uint32_t MaxConvertibleFrames();
// Puts a raw animation (any raw/static PANM, e.g. a partial skeleton) on a native template's skeleton,
// matching bones by name. Template bones missing from the source keep the template's frame-0 pose.
std::vector<Byte> Rewrite(const RetailPanm::Animation& source, std::span<const Byte> templatePanm, size_t* matchedBones = nullptr);
}
