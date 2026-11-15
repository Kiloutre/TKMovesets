#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// Internal, versioned lossless PANM representation used by CompactZstd.
// Unknown/unsupported PANMs may be stored opaquely by the archive encoder.
namespace CompactPanm {
using Bytes = std::vector<uint8_t>;
inline constexpr size_t MaximumSize = 32u << 20;
// At most 32 bytes of range/model/padding overhead per transformed track, plus the header.
inline constexpr size_t MaximumOverhead = 24 + 32 * 16384;
// Encoder choices do not change the PCF1 decoding contract. Exposed for offline tuning.
struct EncodeSettings {
    uint16_t rawModes = 0x3f1f, correctionModes = 0x1f, packedModes = 0x1f;
    uint8_t quaternionChannels = 0x78;
    int scoreLevel = 9; // 0 uses byte-entropy estimation instead of trial compression.
    uint32_t sampleFrames = 0; // 0 scores the complete channel.
};
EncodeSettings FastSettings();
Bytes Encode(std::span<const uint8_t> panm);
Bytes Encode(std::span<const uint8_t> panm, const EncodeSettings& settings);
// Throws std::invalid_argument on malformed input. Output size is supplied by the bounded archive.
void Decode(std::span<const uint8_t> encoded, std::span<uint8_t> output);
}
