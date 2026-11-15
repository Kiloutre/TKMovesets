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
}
