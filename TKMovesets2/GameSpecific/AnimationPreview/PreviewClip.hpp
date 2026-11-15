#pragma once
#include "PreviewModel.hpp"
#include <span>
#include <string>
#include <vector>

namespace AnimPreview {
// One body animation as frames of the 23 T7 channels (0xC8 form), whatever its source format.
struct Clip {
    std::vector<Channels> frames;
    std::string format; // "T7 0xC8", "T7 0x64" or "T8 PANM"
};
// Longest animation the preview decodes (0x64 decoding cost grows with the frame count squared).
inline constexpr uint32_t MaxFrames = 20000;
// T7 0xC8 is read as is; T7 0x64 keyframe triples become 0xC8 Euler angles (as T7AnimToRetailT8 reads them); a retail
// T8 PANM goes through RetailT8AnimToT7::ToChannels. Only the body channels are kept: hands, face and other MOTA-like
// tracks are not previewed. Throws std::exception with a readable message on anything else.
Clip DecodeClip(std::span<const Byte> animation);
}
