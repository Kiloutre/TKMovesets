#include "T7WingToRetailT8.hpp"
#include "AnimationReader.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <stdexcept>

namespace T7WingToRetailT8 {
namespace {
struct RigBone { const char* name; std::array<float, 3> rest; };
struct Rig { const char* source; std::vector<RigBone> bones; };
const std::vector<Rig>& Rigs() {
    static const std::vector<Rig> rigs = {
#include "T7WingToRetailT8Rigs.inc"
    };
    return rigs;
}
using Quat = std::array<double, 4>; // x y z w
Quat Mul(const Quat& a, const Quat& b) {
    return {a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1],
            a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0],
            a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3],
            a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2]};
}
// 0x64: the inverse of the axis-angle rotation (axis (cos a sin b, -sin a, cos a cos b), angle c); 0xC8 stores that
// inverse as ZYX Euler angles.
Quat Rotation(const float* v, bool keyframed) {
    if (keyframed) {
        const double a = v[0], b = v[1], c = v[2], s = std::sin(c / 2);
        return {-std::cos(a) * std::sin(b) * s, std::sin(a) * s, -std::cos(a) * std::cos(b) * s, std::cos(c / 2)};
    }
    const Quat x{std::sin(v[0] / 2.0), 0, 0, std::cos(v[0] / 2.0)}, y{0, std::sin(v[1] / 2.0), 0, std::cos(v[1] / 2.0)},
        z{0, 0, std::sin(v[2] / 2.0), std::cos(v[2] / 2.0)};
    return Mul(Mul(z, y), x);
}
std::vector<uint32_t> Descriptors(std::span<const Byte> animation) {
    const auto layout = TAnimUtils::ReadAnimationLayout(animation);
    std::vector<uint32_t> result;
    for (uint16_t i = 0; i < layout.bones; ++i) {
        uint32_t d = 0;
        if (layout.type == 0x64) {
            uint16_t v; memcpy(&v, animation.data() + 4 + i * 2, 2); d = layout.swapped ? _byteswap_ushort(v) : v;
        } else {
            uint32_t v; memcpy(&v, animation.data() + 8 + i * 4, 4); d = layout.swapped ? _byteswap_ulong(v) : v;
        }
        result.push_back(d);
    }
    return result;
}
struct Group { size_t position; std::optional<size_t> rotation, scale; };
}

std::vector<Byte> Convert(std::span<const Byte> animation, std::string* chosen) {
    const auto descriptors = Descriptors(animation);
    std::vector<Group> groups;
    for (size_t c = 0; c < descriptors.size(); ++c) {
        const auto d = descriptors[c];
        if (d == 0x0B) groups.push_back({c});
        else if (d == 0x07 && !groups.empty() && !groups.back().rotation) groups.back().rotation = c;
        else if (d == 0x03 && !groups.empty() && groups.back().rotation && !groups.back().scale) groups.back().scale = c;
        else throw std::invalid_argument("T7 wing channels are not bones of position, rotation and optional scale");
    }
    for (const auto& g : groups) if (!g.rotation) throw std::invalid_argument("T7 wing bone without a rotation");
    const auto source = RetailPanm::DecodeT7(animation, 30000);
    auto at = [&](uint32_t frame, size_t channel) { return &source.values[(size_t(frame) * source.channels + channel) * 3]; };
    // The rig with this bone count whose rest offsets the frame-0 positions match best, within RestTolerance.
    const Rig* rig = nullptr; float best = RestTolerance;
    for (const auto& candidate : Rigs()) {
        if (candidate.bones.size() != groups.size()) continue;
        float worst = 0;
        for (size_t b = 0; b < groups.size(); ++b)
            for (size_t k = 0; k < 3; ++k) worst = (std::max)(worst, std::abs(at(0, groups[b].position)[k] * 0.1f - candidate.bones[b].rest[k]));
        if (worst <= best) { best = worst; rig = &candidate; }
    }
    if (!rig) throw std::invalid_argument("No T8 wing rig has this T7 wing animation's bone offsets");
    if (chosen) *chosen = rig->source;
    RetailPanm::Animation out; out.frames = source.frames;
    for (size_t b = 0; b < groups.size(); ++b) {
        RetailPanm::Track track; track.name = rig->bones[b].name; track.options.clear();
        track.samples.resize(source.frames);
        for (uint32_t f = 0; f < source.frames; ++f) {
            auto& s = track.samples[f];
            const float* p = at(f, groups[b].position);
            for (size_t k = 0; k < 3; ++k) s.translation[k] = p[k] * 0.1f;
            const auto q = Rotation(at(f, *groups[b].rotation), source.keyframed);
            const double n = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
            if (!(n > 1e-10)) throw std::invalid_argument("Degenerate T7 wing rotation");
            for (size_t k = 0; k < 4; ++k) s.rotation[k] = static_cast<float>(q[k] / n);
            if (groups[b].scale) { const float* m = at(f, *groups[b].scale); s.scale = {m[0], m[1], m[2]}; }
        }
        out.tracks.push_back(std::move(track));
    }
    return RetailPanm::Encode(out);
}
}
