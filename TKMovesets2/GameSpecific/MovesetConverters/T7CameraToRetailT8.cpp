#include "T7CameraToRetailT8.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <numbers>
#include <stdexcept>

namespace T7CameraToRetailT8 {
namespace {
using Quat = std::array<double, 4>; // x y z w
using Vec = std::array<double, 3>;
Quat Mul(const Quat& a, const Quat& b) {
    return {a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1],
            a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0],
            a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3],
            a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2]};
}
// ZYX Euler: q = Rz * Ry * Rx.
Quat FromEuler(double x, double y, double z) {
    const Quat qx{std::sin(x/2), 0, 0, std::cos(x/2)}, qy{0, std::sin(y/2), 0, std::cos(y/2)}, qz{0, 0, std::sin(z/2), std::cos(z/2)};
    return Mul(Mul(qz, qy), qx);
}
std::optional<Vec> Normalized(const Vec& v) {
    const double n = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (n < 1e-12) return {};
    return Vec{v[0]/n, v[1]/n, v[2]/n};
}
Vec Cross(const Vec& a, const Vec& b) { return {a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0]}; }
// Rotation whose columns are the basis vectors x, y, z.
Quat FromBasis(const Vec& x, const Vec& y, const Vec& z) {
    const double m00 = x[0], m01 = y[0], m02 = z[0], m10 = x[1], m11 = y[1], m12 = z[1], m20 = x[2], m21 = y[2], m22 = z[2];
    const double trace = m00 + m11 + m22;
    if (trace > 0) { const double s = std::sqrt(trace + 1) * 2; return {(m21 - m12)/s, (m02 - m20)/s, (m10 - m01)/s, s/4}; }
    if (m00 > m11 && m00 > m22) { const double s = std::sqrt(1 + m00 - m11 - m22) * 2; return {s/4, (m01 + m10)/s, (m02 + m20)/s, (m21 - m12)/s}; }
    if (m11 > m22) { const double s = std::sqrt(1 + m11 - m00 - m22) * 2; return {(m01 + m10)/s, s/4, (m12 + m21)/s, (m02 - m20)/s}; }
    const double s = std::sqrt(1 + m22 - m00 - m11) * 2; return {(m02 + m20)/s, (m12 + m21)/s, s/4, (m10 - m01)/s};
}
uint32_t Load32(std::span<const Byte> b, size_t at, bool swap) {
    if (at + 4 > b.size()) throw std::invalid_argument("T7 camera animation is truncated");
    uint32_t v; memcpy(&v, b.data() + at, 4); return swap ? _byteswap_ulong(v) : v;
}
struct Layout { std::vector<size_t> positions; std::optional<size_t> rotation, smoothing, roll; size_t zoom = 0; bool lookAt = false; };
std::optional<Layout> Find(const Camera& c) {
    Layout l;
    for (size_t i = 0; i < c.descriptors.size(); ++i) {
        const auto d = c.descriptors[i];
        if (d == Position) l.positions.push_back(i);
        else if (d == Rotation && !l.rotation) l.rotation = i;
        else if (d == Smoothing && !l.smoothing) l.smoothing = i;
        else if (d == Roll && !l.roll) l.roll = i;
    }
    const auto zoom = std::find(c.descriptors.begin(), c.descriptors.end(), Zoom);
    if (zoom == c.descriptors.end() || l.positions.empty()) return {};
    l.zoom = size_t(zoom - c.descriptors.begin());
    l.lookAt = !l.rotation && l.positions.size() >= 2;
    if (!l.rotation && !l.lookAt) return {};
    return l;
}
}

Camera Read(std::span<const Byte> b, uint32_t maxFrames) {
    if (b.size() < 8) throw std::invalid_argument("T7 camera animation is truncated");
    if (b[0] != 0xC8 && b[1] != 0xC8) throw std::invalid_argument("T7 camera animation is not a 0xC8 clip");
    const bool swap = b[1] == 0xC8 && b[0] == 0; // big endian: 00 C8
    uint16_t channels; memcpy(&channels, b.data() + 2, 2); if (swap) channels = _byteswap_ushort(channels);
    Camera c; c.frames = Load32(b, 4, swap);
    if (!channels || channels > 64 || !c.frames || c.frames > maxFrames) throw std::invalid_argument("T7 camera animation has an invalid size");
    for (uint16_t i = 0; i < channels; ++i) c.descriptors.push_back(Load32(b, 8 + i * 4, swap));
    const size_t start = 8 + size_t(channels) * 4, count = size_t(c.frames) * channels * 3;
    if (start + count * 4 > b.size()) throw std::invalid_argument("T7 camera animation is truncated");
    c.values.resize(count);
    for (size_t i = 0; i < count; ++i) {
        const auto raw = Load32(b, start + i * 4, swap); float f; memcpy(&f, &raw, 4);
        if (!std::isfinite(f)) throw std::invalid_argument("Nonfinite T7 camera channel");
        c.values[i] = f;
    }
    return c;
}

bool Convertible(const Camera& c) {
    return c.frames && c.values.size() == size_t(c.frames) * c.descriptors.size() * 3 && Find(c).has_value();
}

std::vector<Byte> Convert(const Camera& c) {
    const auto layout = Find(c);
    if (!layout || c.values.size() != size_t(c.frames) * c.descriptors.size() * 3) throw std::invalid_argument("Unsupported T7 camera channel layout");
    const auto& l = *layout;
    const size_t stride = c.descriptors.size() * 3;
    auto at = [&](uint32_t frame, size_t channel) { return &c.values[frame * stride + channel * 3]; };
    // Look-at layouts: the first position is the target, the second the camera.
    const auto cameraChannel = l.lookAt ? l.positions[1] : l.positions[0];
    std::vector<RetailPanm::Sample> transform(c.frames), parameters;
    std::vector<float> fov(c.frames);
    Quat previous{0, 0, 0, 1};
    std::optional<Vec> previousX;
    for (uint32_t f = 0; f < c.frames; ++f) {
        const float* p = at(f, cameraChannel);
        Quat q = previous;
        if (l.lookAt) {
            // Local -Z looks at the target, world up is +Y.
            const float* t = at(f, l.positions[0]);
            if (const auto forward = Normalized({double(t[0]) - p[0], double(t[1]) - p[1], double(t[2]) - p[2]})) {
                const Vec z{-(*forward)[0], -(*forward)[1], -(*forward)[2]};
                Vec x = Normalized(Cross({0, 1, 0}, z)).value_or(previousX.value_or(Vec{1, 0, 0}));
                previousX = x;
                q = FromBasis(x, Cross(z, x), z);
            }
        } else {
            const float* r = at(f, *l.rotation);
            q = FromEuler(r[0], r[1], r[2]);
        }
        // Roll: degrees, about the world Z axis, before the camera rotation (native T8 look-at cameras within 1.5e-4
        // rad; Polaris's radians after the rotation left up to 0.26 rad).
        if (l.roll) if (const double roll = at(f, *l.roll)[0]; roll != 0) {
            const double a = roll * std::numbers::pi / 180;
            q = Mul({0, 0, std::sin(a/2), std::cos(a/2)}, q);
        }
        if (q[0]*previous[0] + q[1]*previous[1] + q[2]*previous[2] + q[3]*previous[3] < 0) for (auto& v : q) v = -v;
        previous = q;
        const double n = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
        auto& s = transform[f];
        for (size_t k = 0; k < 4; ++k) s.rotation[k] = static_cast<float>(q[k] / n);
        for (size_t k = 0; k < 3; ++k) s.translation[k] = p[k] * 0.1f; // mm -> cm
        fov[f] = at(f, l.zoom)[0] * 0.5625f;
        if (l.smoothing) {
            // Native Camera_Param tracks animate scale x only (packed flags 1, which some native swing tracks use for a
            // uniform scale): y and z stay at their first value so the track reads the same either way. 381 of 406 T7
            // smoothing channels vary x only, 22 nothing, 3 all three.
            RetailPanm::Sample param; param.rotation = {0, 0, 0, 1};
            const float* m = at(f, *l.smoothing); const float* first = at(0, *l.smoothing);
            param.scale = {m[0], first[1], first[2]};
            parameters.push_back(param);
        }
    }
    std::vector<RetailPanm::GroupTrack> tracks;
    RetailPanm::GroupTrack t; t.group = 1; t.bone = "cam_transform"; t.name = "Transform"; t.type = 1; t.encoding = 3; t.samples = c.frames;
    t.options = {{3, 1}, {4, 1}}; t.data = RetailPanm::PackTransforms(transform);
    tracks.push_back(t);
    if (l.smoothing) {
        RetailPanm::GroupTrack m; m.group = 1; m.bone = "Camera_Param"; m.name = "Transform"; m.type = 1; m.encoding = 3; m.samples = c.frames;
        m.options = {{1, 1}, {2, 1}, {4, 1}}; m.data = RetailPanm::PackTransforms(parameters);
        tracks.push_back(m);
    }
    RetailPanm::GroupTrack v; v.group = 5; v.bone = "cam_transform"; v.name = "FieldOfView"; v.type = 3;
    if (std::all_of(fov.begin(), fov.end(), [&](float x) { return x == fov[0]; })) {
        v.encoding = 2; v.samples = 1; v.data.resize(4); memcpy(v.data.data(), &fov[0], 4);
    } else { v.encoding = 3; v.samples = c.frames; v.data = RetailPanm::PackScalars(fov); }
    tracks.push_back(v);
    return RetailPanm::EncodeGroups(c.frames, tracks);
}
}
