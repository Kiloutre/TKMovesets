#include "T7FaceToRetailT8.hpp"
#include "AnimationReader.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace T7FaceToRetailT8 {
namespace {
// T7 bone k (channels 2k, 2k+1) -> legacy T8 face bone. Matched on every native T8 file of this rig: 43 of 43 bones on
// all 10 files with a T7 source (com x3, hms x6 = T7 Emilie 5#11..16, test x1).
constexpr const char* BoneNames[Bones] = {
    "L_Mayu1_Joint", "L_Mayu2_Joint", "L_Mayu3_Joint", "R_Mayu1_Joint", "R_Mayu2_Joint", "R_Mayu3_Joint",
    "L_Eyecorner", "R_Eyecorner", "L_UwaMabuta_Joint", "L_UpEyelid1", "L_UpEyelid2", "R_UwaMabuta_Joint",
    "R_UpEyelid1", "R_UpEyelid2", "L_Eye_Joint", "R_Eye_Joint", "L_SitaMabuta_Joint", "L_LowEyelid1",
    "L_LowEyelid2", "R_SitaMabuta_Joint", "R_LowEyelid1", "R_LowEyelid2", "L_Hoho_Joint", "L_Hana_Joint",
    "L_Kobana_Joint", "L_Chobo_Joint", "R_Hoho_Joint", "R_Hana_Joint", "R_Kobana_Joint", "R_Chobo_Joint",
    "L_LipU_Joint", "R_LipU_Joint", "M_LipU_Joint", "Jaw_Joint", "L_LipD_Joint", "R_LipD_Joint",
    "M_LipD_Joint", "L_LipE_Joint", "R_LipE_Joint", "Bero1_Joint", "Bero1_Joint_pos", "Bero2_Joint",
    "Bero3_Joint"};
using Quat = std::array<double, 4>; // x y z w
Quat Mul(const Quat& a, const Quat& b) {
    return {a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1],
            a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0],
            a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3],
            a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2]};
}
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
        if (layout.type == 0x64) { uint16_t v; memcpy(&v, animation.data() + 4 + i * 2, 2); result.push_back(layout.swapped ? _byteswap_ushort(v) : v); }
        else { uint32_t v; memcpy(&v, animation.data() + 8 + i * 4, 4); result.push_back(layout.swapped ? _byteswap_ulong(v) : v); }
    }
    return result;
}
}

std::vector<Byte> Convert(std::span<const Byte> animation) {
    const auto descriptors = Descriptors(animation);
    bool layout = descriptors.size() == Bones * 2 + 4;
    for (size_t c = 0; layout && c < descriptors.size(); ++c)
        layout = descriptors[c] == (c >= Bones * 2 ? 0x03u : c % 2 ? 0x07u : 0x0Bu);
    if (!layout) throw std::invalid_argument("T7 face animation is not the 43-bone (position, rotation) layout");
    // 0x64 decoding cost grows with frames squared; T7 faces run to about 7,400 frames.
    const auto source = RetailPanm::DecodeT7(animation, 20000);
    auto at = [&](uint32_t frame, size_t channel) { return &source.values[(size_t(frame) * source.channels + channel) * 3]; };
    std::vector<RetailPanm::GroupTrack> tracks;
    for (size_t b = 0; b < Bones; ++b) {
        std::vector<RetailPanm::Sample> samples(source.frames);
        for (uint32_t f = 0; f < source.frames; ++f) {
            auto& s = samples[f];
            const float* p = at(f, b * 2);
            for (size_t k = 0; k < 3; ++k) s.translation[k] = p[k] * 0.1f;
            auto q = Rotation(at(f, b * 2 + 1), source.keyframed);
            const double n = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
            if (!(n > 1e-10)) throw std::invalid_argument("Degenerate T7 face rotation");
            for (size_t k = 0; k < 4; ++k) s.rotation[k] = static_cast<float>(q[k] / n);
        }
        RetailPanm::GroupTrack t; t.group = 1; t.bone = BoneNames[b]; t.name = "Transform"; t.type = 1; t.options = {{3, 1}, {4, 1}};
        const bool still = std::all_of(samples.begin(), samples.end(), [&](const RetailPanm::Sample& s) {
            return !memcmp(&s, &samples[0], sizeof(s)); });
        if (still) {
            t.encoding = 2; t.samples = 1; t.data.resize(sizeof(RetailPanm::Sample)); memcpy(t.data.data(), &samples[0], sizeof(RetailPanm::Sample));
        } else { t.encoding = 3; t.samples = source.frames; t.data = RetailPanm::PackTransforms(samples); }
        tracks.push_back(std::move(t));
    }
    return RetailPanm::EncodeGroups(source.frames, tracks);
}
}
