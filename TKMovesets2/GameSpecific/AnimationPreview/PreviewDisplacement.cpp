#include "PreviewDisplacement.hpp"
#include <algorithm>

namespace AnimPreview {
using Kind = Displacement::Kind;

std::optional<Kind> T7DisplacementKind(uint32_t id) {
    switch (id) {
    case 0x81E1: return Kind::HorizontalScale;
    case 0x81E2: return Kind::VerticalScale;
    case 0x8206: return Kind::PushForward;
    case 0x8205: return Kind::PushSide;
    default: return std::nullopt;
    }
}

std::optional<Kind> T8DisplacementKind(uint32_t id) {
    switch (id) {
    case 0x8251: return Kind::HorizontalScale;
    case 0x8252: return Kind::VerticalScale;
    case 0x8276: return Kind::PushForward;
    case 0x8275: return Kind::PushSide;
    default: return std::nullopt;
    }
}

int DisplacementFrame(uint32_t frame) { return (std::max)(1, int(frame & 0x7FFF)); }

float DisplacementValue(Kind kind, uint32_t raw, bool t8) {
    // Multipliers: s16 percent (T8 handler: (float)(s16)param * 0.01). Pushes: T8 reads the low 16 bits as s16, T7 the
    // signed 32-bit value.
    if (t8 || kind == Kind::HorizontalScale || kind == Kind::VerticalScale) return float(int16_t(raw & 0xFFFF));
    return float(int32_t(raw));
}

std::vector<Channels> ApplyDisplacements(std::span<const Channels> frames, std::span<const Displacement> displacements) {
    std::vector<Channels> out(frames.begin(), frames.end());
    if (frames.empty() || displacements.empty()) return out;
    std::vector<Displacement> sorted(displacements.begin(), displacements.end());
    std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.frame < b.frame; });
    float horizontal = 1, vertical = 1, pushX = 0, pushZ = 0;
    // Running root position and height: the animation's motion, scaled by the multipliers in force.
    float rootX = frames[0][0], rootZ = frames[0][2], height = frames[0][4];
    size_t next = 0;
    for (size_t f = 0; f < frames.size(); ++f) {
        // Properties of this frame act on this frame's motion (from the previous frame to this one).
        for (; next < sorted.size() && sorted[next].frame <= int(f) + 1; ++next) {
            const auto& d = sorted[next];
            switch (d.kind) {
            case Kind::HorizontalScale: horizontal = d.value / 100; break;
            case Kind::VerticalScale: vertical = d.value / 100; break;
            case Kind::PushForward: pushZ += d.value; break;
            case Kind::PushSide: pushX += d.value; break;
            }
        }
        if (f > 0) {
            rootX += (frames[f][0] - frames[f - 1][0]) * horizontal;
            rootZ += (frames[f][2] - frames[f - 1][2]) * horizontal;
            height += (frames[f][4] - frames[f - 1][4]) * vertical;
        }
        out[f][0] = rootX + pushX;
        out[f][2] = rootZ + pushZ;
        out[f][4] = height;
    }
    return out;
}
}
