#include "PreviewClip.hpp"
#include "RetailPanm.hpp"
#include "RetailT8AnimToT7.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace AnimPreview {
namespace {
using Quat = std::array<double, 4>; // x y z w
std::array<double, 3> ToEuler(const Quat& q) {
    return {std::atan2(2 * (q[3] * q[0] + q[1] * q[2]), 1 - 2 * (q[0] * q[0] + q[1] * q[1])),
            std::asin(std::fmax(-1.0, std::fmin(1.0, 2 * (q[3] * q[1] - q[2] * q[0])))),
            std::atan2(2 * (q[3] * q[2] + q[0] * q[1]), 1 - 2 * (q[1] * q[1] + q[2] * q[2]))};
}
// Channels 0 (GlobalPos), 1 (LocalPos) and 6 (SpineFlex) are positions; the others rotations.
bool IsPosition(int channel) { return channel == 0 || channel == 1 || channel == 6; }

Clip FromT7(const RetailPanm::T7Animation& source, const char* format) {
    if (source.channels < 23) throw std::runtime_error("Not a body animation (fewer than 23 channels)");
    Clip clip; clip.format = format;
    clip.frames.resize(source.frames);
    for (uint32_t f = 0; f < source.frames; ++f) {
        auto& out = clip.frames[f];
        for (int ch = 0; ch < 23; ++ch) {
            const float* v = &source.values[(size_t(f) * source.channels + ch) * 3];
            if (!source.keyframed || IsPosition(ch)) { out[ch * 3] = v[0]; out[ch * 3 + 1] = v[1]; out[ch * 3 + 2] = v[2]; continue; }
            // 0x64 rotations: axis (cos a sin b, -sin a, cos a cos b), angle c; 0xC8 stores the inverse rotation as Euler
            // (T7AnimToRetailT8.cpp, ReadFrame).
            const double a = v[0], b = v[1], c = v[2], s = std::sin(c / 2);
            const Quat inverse{-std::cos(a) * std::sin(b) * s, std::sin(a) * s, -std::cos(a) * std::cos(b) * s, std::cos(c / 2)};
            const auto e = ToEuler(inverse);
            for (int i = 0; i < 3; ++i) out[ch * 3 + i] = float(e[i]);
        }
    }
    return clip;
}
}

Clip DecodeClip(std::span<const Byte> animation) {
    if (animation.size() < 4) throw std::runtime_error("Animation is empty");
    const uint16_t tag = uint16_t(animation[0] | animation[1] << 8);
    if (tag == 0x64 || tag == 0xC8 || tag == 0x6400 || tag == 0xC800) {
        const auto decoded = RetailPanm::DecodeT7(animation, MaxFrames);
        return FromT7(decoded, decoded.keyframed ? "T7 0x64" : "T7 0xC8");
    }
    const auto panm = RetailPanm::Decode(animation);
    if (panm.frames > MaxFrames) throw std::runtime_error("Animation is too long to preview");
    return FromT7(RetailT8AnimToT7::ToChannels(panm), "T8 PANM");
}
}
