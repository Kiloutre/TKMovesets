#include "PreviewHitbox.hpp"
#include <algorithm>

namespace AnimPreview {
namespace {
struct Limb { uint32_t id; const char* bone; std::array<float, 3> rest; };
// Rest positions (armature space) of the hit points on the mannequin: hand and foot centres, elbows and knees.
constexpr Limb Limbs[] = {
    {0x9, "R_Hand", {-0.89f, 1.51f, 0}}, {0xD, "L_Hand", {0.89f, 1.51f, 0}},
    {0x10, "R_Foot", {-0.1f, 0.05f, 0.08f}}, {0x13, "L_Foot", {0.1f, 0.05f, 0.08f}},
    {0x8, "R_ForeArm", {-0.558f, 1.518f, 0}}, {0xC, "L_ForeArm", {0.558f, 1.518f, 0}},
    {0xF, "R_Leg", {-0.1f, 0.55f, 0}}, {0x12, "L_Leg", {0.1f, 0.55f, 0}},
};
constexpr float DefaultRadius = 0.09f;
}

std::optional<std::array<float, 3>> LimbPoint(const Model& model, std::span<const Mat4> pose, uint32_t limb) {
    for (const auto& l : Limbs) {
        if (l.id != limb) continue;
        const int bone = model.Find(l.bone);
        if (bone < 0 || size_t(bone) >= pose.size()) return std::nullopt;
        return (pose[bone] * model.bones[bone].restInverse).Point(l.rest);
    }
    return std::nullopt;
}

std::vector<HitShape> HitShapes(const Model& model, std::span<const Mat4> pose, std::span<const Mat4> previous,
    std::span<const HitboxSlot> slots, int frame) {
    std::vector<HitShape> shapes;
    for (const auto& slot : slots) {
        if (!slot.location || frame < slot.start || frame > slot.last) continue;
        const auto a = LimbPoint(model, pose, slot.location & 0xFF);
        if (!a) continue;
        // The game's radius units are not known: values below 1 are read as metres, larger ones as game units (mm).
        float radius = slot.radius > 0 ? (slot.radius < 1 ? slot.radius : slot.radius / 1000) : DefaultRadius;
        radius = (std::min)(radius, 0.4f);
        HitShape shape{*a, *a, radius};
        if (slot.verticalHeight > 0) {
            const float half = (slot.verticalHeight < 10 ? slot.verticalHeight : slot.verticalHeight / 1000) / 2;
            shape.a[1] -= half; shape.b[1] += half;
        } else if (const uint32_t limbB = (slot.location >> 8) & 0xFF) {
            if (const auto b = LimbPoint(model, pose, limbB)) shape.b = *b;
        } else if (!previous.empty()) {
            if (const auto p = LimbPoint(model, previous, slot.location & 0xFF)) shape.b = *p;
        }
        shapes.push_back(shape);
    }
    return shapes;
}
}
