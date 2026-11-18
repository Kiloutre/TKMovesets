#include "T7HandToRetailT8.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace T7HandToRetailT8 {
namespace {
// Native hand skeletons (right, then left): bone order, track options (none) and frame-0 pose of native hand PANMs.
struct SkeletonBone {
    const char* name; RetailPanm::TrackOption options[1];
    std::array<float, 3> scale; std::array<float, 4> rotation; std::array<float, 3> translation;
};
constexpr SkeletonBone SkeletonBones[] = {
#include "T7HandToRetailT8Skeleton.inc"
};
constexpr size_t HandBones = 19;
static_assert(std::size(SkeletonBones) == 2 * HandBones);

// T7 channel -> finger (names without the side prefix). The T7 hand hierarchy lists the index metacarpal first, then
// the thumb joints 1..3 (T8 Thumb0..2), then each finger 1..3, with the middle, ring and pinky metacarpals before their
// fingers. The 15-channel form has no metacarpals.
constexpr const char* Channels19[] = {"Index0", "Thumb0", "Thumb1", "Thumb2", "Index1", "Index2", "Index3",
    "Middle0", "Middle1", "Middle2", "Middle3", "Ring0", "Ring1", "Ring2", "Ring3", "Pinky0", "Pinky1", "Pinky2", "Pinky3"};
constexpr const char* Channels15[] = {"Thumb0", "Thumb1", "Thumb2", "Index1", "Index2", "Index3",
    "Middle1", "Middle2", "Middle3", "Ring1", "Ring2", "Ring3", "Pinky1", "Pinky2", "Pinky3"};

using Quat = std::array<double, 4>; // x y z w
Quat Mul(const Quat& a, const Quat& b) {
    return {a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1],
            a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0],
            a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3],
            a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2]};
}
// The T8 rotation of one channel sample. 0x64: the inverse of the axis-angle rotation, axis (cos a sin b, -sin a,
// cos a cos b), angle c. 0xC8 already stores that inverse, as ZYX Euler angles (q = Rz * Ry * Rx).
Quat Rotation(const float* v, bool keyframed) {
    if (keyframed) {
        const double a = v[0], b = v[1], c = v[2], s = std::sin(c / 2);
        return {-std::cos(a) * std::sin(b) * s, std::sin(a) * s, -std::cos(a) * std::cos(b) * s, std::cos(c / 2)};
    }
    const Quat x{std::sin(v[0] / 2.0), 0, 0, std::cos(v[0] / 2.0)}, y{0, std::sin(v[1] / 2.0), 0, std::cos(v[1] / 2.0)},
        z{0, 0, std::sin(v[2] / 2.0), std::cos(v[2] / 2.0)};
    return Mul(Mul(z, y), x);
}
}

bool Convertible(const RetailPanm::T7Animation& source) {
    return source.frames && (source.channels == std::size(Channels19) || source.channels == std::size(Channels15)) &&
        source.values.size() == size_t(source.frames) * source.channels * 3;
}

std::vector<Byte> Convert(const RetailPanm::T7Animation& source, Side side) {
    if (!Convertible(source)) throw std::invalid_argument("T7 hand animation has neither 19 nor 15 finger channels");
    const auto* channels = source.channels == std::size(Channels19) ? Channels19 : Channels15;
    const std::string prefix = side == Side::Left ? "L_" : "R_";
    RetailPanm::Animation out; out.frames = source.frames;
    for (size_t i = 0; i < HandBones; ++i) {
        const auto& bone = SkeletonBones[(side == Side::Left ? HandBones : 0) + i];
        RetailPanm::Track track; track.name = bone.name; track.options.clear();
        for (const auto& option : bone.options) if (option.field) track.options.push_back(option);
        RetailPanm::Sample rest; rest.scale = bone.scale; rest.rotation = bone.rotation; rest.translation = bone.translation;
        int channel = -1;
        for (int c = 0; c < source.channels; ++c) if (prefix + channels[c] == track.name) channel = c;
        if (channel < 0) { track.samples = {rest}; out.tracks.push_back(std::move(track)); continue; }
        track.samples.assign(source.frames, rest);
        for (uint32_t frame = 0; frame < source.frames; ++frame) {
            auto q = Rotation(&source.values[(size_t(frame) * source.channels + channel) * 3], source.keyframed);
            // The left hand mirrors the right one: native T8 left hands are the right-hand rotation with x and y negated
            // (T7 Kazuya against native T8 Kazuya: 50 left and 46 right same-length pairs within 1e-4 rad on average).
            if (side == Side::Left) { q[0] = -q[0]; q[1] = -q[1]; }
            const double n = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
            if (!(n > 1e-10)) throw std::invalid_argument("Degenerate T7 hand rotation");
            for (size_t k = 0; k < 4; ++k) track.samples[frame].rotation[k] = static_cast<float>(q[k] / n);
        }
        out.tracks.push_back(std::move(track));
    }
    return RetailPanm::Encode(out);
}

std::vector<std::span<const Byte>> MotaAnimations(std::span<const Byte> mota) {
    // MOTA header (StructsT7::MotaHeader): "MOTA", +0x0C u32 count, +0x11 endian byte (non-zero: little endian),
    // +0x14 u32 offsets from the archive start.
    if (mota.size() < 0x14 || memcmp(mota.data(), "MOTA", 4)) throw std::invalid_argument("Invalid T7 MOTA archive");
    const bool little = mota[0x11] != 0;
    auto u32 = [&](size_t at) {
        if (at + 4 > mota.size()) throw std::invalid_argument("T7 MOTA archive is truncated");
        uint32_t v; memcpy(&v, mota.data() + at, 4);
        return little ? v : _byteswap_ulong(v);
    };
    const auto count = u32(0x0C);
    if (count > 0x10000 || 0x14 + uint64_t(count) * 4 > mota.size()) throw std::invalid_argument("Invalid T7 MOTA animation count");
    std::vector<std::span<const Byte>> result;
    for (uint32_t i = 0; i < count; ++i) {
        const auto offset = u32(0x14 + i * 4);
        if (offset < 0x14 + uint64_t(count) * 4 || offset >= mota.size()) throw std::invalid_argument("T7 MOTA animation outside its archive");
        result.push_back(mota.subspan(offset));
    }
    return result;
}
}
