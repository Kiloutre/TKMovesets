#pragma once
#include "PreviewModel.hpp"
#include <algorithm>
#include <optional>
#include <span>
#include <vector>

// Animation preview: approximate hitboxes of the move, drawn as semi-transparent red shapes on its active frames.
// A slot's location packs limb A in byte 0 and limb B in byte 1 (0: the shape sweeps limb A from its previous-frame
// position). Limb ids, from the retail field codec (RetailFieldCodecs.hpp, move-name correlation): 0x9 right hand,
// 0xD left hand, 0x10 right foot, 0x13 left foot (confirmed); 0x8 / 0xC right / left forearm and 0xF / 0x12 right /
// left shin (hypotheses, drawn at the elbow and knee). T7's hitbox_location uses the same ids. Other ids are not drawn.
// Shapes are placed on the mannequin, not the game's skeleton, and the default radius is a guess: approximate only.
namespace AnimPreview {
struct HitboxSlot {
    int start = 0, last = 0;   // 1-based active frames, inclusive
    uint32_t location = 0;
    float radius = 0;          // 0: default
    float verticalHeight = 0;  // > 0: vertical segment of that height centred on limb A
    bool operator==(const HitboxSlot&) const = default;
};
// Reaction moves of one hit condition's reaction list (move ids, aliases resolved; -1: none), for the opponent
// mannequin. Neither game has an airborne slot: airborne reactions come from hit conditions requiring an airborne
// opponent (the editors' MoveHitReactions).
enum class Facing { Front, Back, Left, Right }; // side of the opponent the hit lands on
// Pushback of a reaction list, raw game values. Read as: offsets[i] is the distance moved on frame i of the reaction,
// then `displacement` in total, spread evenly over `duration` more frames, in the animations' position units (mm). Unverified
// against the game.
struct Pushback {
    int duration = 0;
    int displacement = 0;
    std::vector<int> offsets;
    bool operator==(const Pushback&) const = default;
    // Distance pushed (mm) after `frames` frames of the reaction (fractional: within a frame, linearly).
    float Distance(float frames) const {
        float distance = 0;
        for (size_t i = 0; i < offsets.size(); ++i) {
            if (frames <= float(i)) return distance;
            distance += float(offsets[i]) * (std::min)(1.0f, frames - float(i));
        }
        const float linear = frames - float(offsets.size());
        if (duration <= 0 || linear <= 0) return distance;
        return distance + float(displacement) * (linear >= float(duration) ? 1.0f : linear / float(duration));
    }
};
struct ReactionMoves {
    int32_t standing = -1, counterhit = -1, leftSide = -1, rightSide = -1, backturned = -1;
    Pushback frontPushback, counterhitPushback, leftPushback, rightPushback, backPushback;
    int damage = 0; // of the hit condition these come from
    // Pushback for the side the hit lands on (counter hit: front only), as the reaction moves.
    const Pushback& PushbackFor(bool isCounterhit, Facing facing) const {
        return facing == Facing::Back ? backPushback : facing == Facing::Left ? leftPushback : facing == Facing::Right ? rightPushback
            : isCounterhit ? counterhitPushback : frontPushback;
    }
    // Reaction for the side the hit lands on, then counter hit (front only, as the game's front counter-hit slot),
    // falling back to the standing reaction.
    int32_t For(bool isCounterhit, Facing facing) const {
        const int32_t id = facing == Facing::Back ? backturned : facing == Facing::Left ? leftSide : facing == Facing::Right ? rightSide
            : isCounterhit ? counterhit : standing;
        return id >= 0 ? id : standing;
    }
    bool operator==(const ReactionMoves&) const = default;
};

// A capsule (a sphere when a == b), armature space, metres.
struct HitShape { std::array<float, 3> a, b; float radius; };

// Point of a limb on the posed mannequin; none for unknown ids.
std::optional<std::array<float, 3>> LimbPoint(const Model& model, std::span<const Mat4> pose, uint32_t limb);
// Shapes live on `frame` (1-based). `previous`: pose of the frame before (sweeps), may be empty.
std::vector<HitShape> HitShapes(const Model& model, std::span<const Mat4> pose, std::span<const Mat4> previous,
    std::span<const HitboxSlot> slots, int frame);
}
