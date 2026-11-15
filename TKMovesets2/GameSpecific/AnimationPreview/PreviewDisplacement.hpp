#pragma once
#include "PreviewModel.hpp"
#include <optional>
#include <span>
#include <vector>

// Animation preview: the move's extra properties that displace the character, applied to its animation's root motion.
// Only properties whose effect is documented (community reference sheets, retail T8 handler analysis in
// Resources/TKMInterface/editor_t8-retail_property_params.txt) are known:
//   horizontal animation distance multiplier  T7 0x81E1, T8 0x8251  percent of the animation's horizontal root motion
//   vertical animation distance multiplier    T7 0x81E2, T8 0x8252  percent of the animation's vertical motion
//   push player in X (forward / backward)     T7 0x8206, T8 0x8276  signed distance, game units, along the facing
//   push player in Z (sideways)               T7 0x8205, T8 0x8275  signed distance, game units, positive = to the left
// Pushbacks inflicted on self (0x81E8 / 0x8258) and height pushes are not applied: their per-frame effect is not known.
namespace AnimPreview {
struct Displacement {
    enum class Kind : uint8_t { HorizontalScale, VerticalScale, PushForward, PushSide };
    int frame = 1;  // 1-based animation frame the property runs on
    Kind kind = Kind::PushForward;
    float value = 0; // percent for the scales, game units for the pushes
    uint32_t id = 0; // property id, for display
    bool operator==(const Displacement&) const = default;
};

std::optional<Displacement::Kind> T7DisplacementKind(uint32_t id);
std::optional<Displacement::Kind> T8DisplacementKind(uint32_t id);
// Property frame field -> 1-based frame: bit 15 marks "instant" (0x8001), frame 0 runs on the first frame.
int DisplacementFrame(uint32_t frame);
// Property parameter -> percent (multipliers) or game units (pushes).
float DisplacementValue(Displacement::Kind kind, uint32_t raw, bool t8);

// The frames with the displacements applied: the multipliers scale the per-frame motion from their frame on (root
// motion GlobalPos x/z, and the body height LocalPos y), the pushes move the root by their distance on their frame.
// Pushes use the facing at the start of the move (forward is game +Z, left is game +X).
std::vector<Channels> ApplyDisplacements(std::span<const Channels> frames, std::span<const Displacement> displacements);
}
