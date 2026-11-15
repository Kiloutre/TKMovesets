#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include "CompactPanm.hpp"

// Compression type 3: a versioned compact PANM record stream followed by Zstd.
// Non-PANM data is opaque. Decompression finishes before any native pointer binding.
namespace CompactZstd {
inline constexpr size_t MaximumPayload = size_t(1) << 30;
struct EncodeSettings {
    // Explicit settings default to the historical reference profile; Compress(payload) uses the fast profile.
    CompactPanm::EncodeSettings panm;
    int level = 19;
    bool transform = true;
};
std::vector<uint8_t> Compress(std::span<const uint8_t> payload);
std::vector<uint8_t> Compress(std::span<const uint8_t> payload, const EncodeSettings& settings);
// Output has the exact expanded size declared in the validated outer moveset header.
// On failure its contents are unspecified; no exception crosses the legacy codec interface.
bool Decompress(std::span<const uint8_t> compressed, std::span<uint8_t> output) noexcept;
}
