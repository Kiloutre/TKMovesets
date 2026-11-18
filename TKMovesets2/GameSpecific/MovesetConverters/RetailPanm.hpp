#pragma once
#include "GameTypes.h"
#include <array>
#include <span>
#include <string>
#include <vector>

namespace RetailPanm {
struct Sample {
    std::array<float,3> scale{1,1,1};
    std::array<float,4> rotation{0,0,0,1}; // XYZW
    std::array<float,3> translation{};
    float reserved = 0;
};
static_assert(sizeof(Sample)==44);
// Track field 5 references a small table of byte flags (field index, value); most raw native tracks carry {8,1}.
struct TrackOption { uint8_t field; uint8_t value; bool operator==(const TrackOption&) const = default; };
struct Track { std::string name; std::vector<Sample> samples; std::vector<TrackOption> options{{8,1}}; };
struct Animation { uint32_t frames=0; std::vector<Track> tracks; };
// Independent FlatBuffers writer with the native PANM layout (back-to-front builder, shared vtables, per-track
// option tables, 8-aligned sample blocks); the game accepts its output. No template animation, game process,
// rig file or external schema is required.
std::vector<Byte> Encode(const Animation& animation);
// Reads native raw/static Transform tracks. Unsupported codecs fail explicitly.
Animation DecodeRaw(std::span<const Byte> bytes);
// Also reads packed (encoding 3) Transform tracks, dequantized to raw samples: what most native body animations use.
Animation Decode(std::span<const Byte> bytes);
// One packed (encoding 3) Transform track's bytes (header, channel ranges, frame-0 sample, bit stream) -> samples.
std::vector<Sample> DecodePackedTrack(std::span<const Byte> track, uint32_t samples);
// Bounded T7 0x64/0xc8 samples; each frame has three floats per source channel. 0xc8 rotation channels are
// ZYX Euler angles; 0x64 (keyframed) rotation channels are the decoded KEF triple (axis angles a, b, rotation c).
struct T7Animation { uint32_t frames; uint16_t channels; std::vector<float> values; bool keyframed = false; };
// `maxFrames`: refuse longer animations before decoding (0x64 decoding cost grows with frames squared).
T7Animation DecodeT7(std::span<const Byte> bytes, uint32_t maxFrames = UINT32_MAX);

// ---- Multi-group PANMs (native camera layout) --------------------------------------------------------
// One track of one bone in one bone group, its sample bytes verbatim. Native cameras: group 1 holds "cam_transform"
// (and "Camera_Param") Transform tracks (type 1), group 5 the "cam_transform" FieldOfView track (type 3, one float per
// sample). Encoding 1 raw, 2 static (one sample), 3 packed (PackTransforms / PackScalars).
struct GroupTrack {
    uint32_t group = 1; std::string bone, name = "Transform"; uint32_t type = 1, encoding = 3, samples = 0;
    std::vector<TrackOption> options; std::vector<Byte> data;
};
// Native camera PANM layout (same builder as Encode, tracks in the given order, grouped by `group` in first-seen
// order; each track's data starts 16-aligned, the block ends 16-aligned after an 8-byte gap). Every native camera
// PANM decoded with DecodeGroups re-encodes byte for byte.
std::vector<Byte> EncodeGroups(uint32_t frames, const std::vector<GroupTrack>& tracks);
std::vector<GroupTrack> DecodeGroups(std::span<const Byte> panm, uint32_t* frames = nullptr);
// Packed (encoding 3) streams: per channel the range and a bit width that keeps the quantization step at or below
// `step` (rotation 1e-7 capped at 24 bits, positions and scale 1e-4, scalars 1e-5); channels whose value never
// changes take no bits.
// Transforms set flags 1 (scale), 4 (rotation), 8 (position) and 2 (rotation or position) for the animated parts,
// and one w-sign bit per frame when the rotation is animated. Some native swing tracks use flag 1 for a uniform scale
// (x only in the stream): animate scale x only when the track must read the same under both meanings.
std::vector<Byte> PackTransforms(std::span<const Sample> samples);
std::vector<Byte> PackScalars(std::span<const float> samples);
std::vector<float> UnpackScalars(std::span<const Byte> track, uint32_t samples);
}
